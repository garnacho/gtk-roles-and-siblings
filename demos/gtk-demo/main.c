#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gtk/gtk.h>

#include <demos.h>

static GtkTextBuffer *info_buffer;
static GtkTextBuffer *source_buffer;

static gchar *current_file = NULL;

enum {
  TITLE_COLUMN,
  FILENAME_COLUMN,
  FUNC_COLUMN,
  ITALIC_COLUMN,
  NUM_COLUMNS
};

typedef struct _CallbackData CallbackData;
struct _CallbackData
{
  GtkTreeModel *model;
  GtkTreePath *path;
};

static void
window_closed_cb (GtkWidget *window, gpointer data)
{
  CallbackData *cbdata = data;
  GtkTreeIter iter;
  gboolean italic;

  gtk_tree_model_get_iter (cbdata->model, &iter, cbdata->path);
  gtk_tree_model_get (GTK_TREE_MODEL (cbdata->model), &iter,
		      ITALIC_COLUMN, &italic,
		      -1);
  if (italic)
    gtk_tree_store_set (GTK_TREE_STORE (cbdata->model), &iter,
			ITALIC_COLUMN, !italic,
			-1);

  gtk_tree_path_free (cbdata->path);
  g_free (cbdata);
}

gboolean
read_line (FILE *stream, GString *str)
{
  int n_read = 0;
  
  flockfile (stream);

  g_string_truncate (str, 0);
  
  while (1)
    {
      int c;
      
      c = getc_unlocked (stream);

      if (c == EOF)
	goto done;
      else
	n_read++;

      switch (c)
	{
	case '\r':
	case '\n':
	  {
	    int next_c = getc_unlocked (stream);
	    
	    if (!(next_c == EOF ||
		  (c == '\r' && next_c == '\n') ||
		  (c == '\n' && next_c == '\r')))
	      ungetc (next_c, stream);
	    
	    goto done;
	  }
	default:
	  g_string_append_c (str, c);
	}
    }

 done:

  funlockfile (stream);

  return n_read > 0;
}


/* Stupid syntax highlighting.
 *
 * No regex was used in the making of this highlighting.
 * It should only work for simple cases.  This is good, as
 * that's all we should have in the demos.
 */
/* This code should not be used elsewhere, except perhaps as an example of how
 * to iterate through a text buffer.
 */
enum {
  STATE_NORMAL,
  STATE_IN_COMMENT,
};

static gchar *tokens[] =
{
  "/*",
  "\"",
  NULL
};

static gchar *types[] =
{
  "static",
  "const ",
  "void",
  "gint",
  "int ",
  "char ",
  "gchar ",
  "gfloat",
  "float",
  "gint8",
  "gint16",
  "gint32",
  "guint",
  "guint8",
  "guint16",
  "guint32",
  "guchar",
  "glong",
  "gboolean" ,
  "gshort",
  "gushort",
  "gulong",
  "gdouble",
  "gldouble",
  "gpointer",
  "NULL",
  "GList",
  "GSList",
  "FALSE",
  "TRUE",
  "FILE ",
  "GtkObject ",
  "GtkColorSelection ",
  "GtkWidget ",
  "GtkButton ",
  "GdkColor ",
  "GdkRectangle ",
  "GdkEventExpose ",
  "GdkGC ",
  "GdkPixbufLoader ",
  "GdkPixbuf ",
  "GError",
  "size_t",
  NULL
};

static gchar *control[] =
{
  " if ",
  " while ",
  " else",
  " do ",
  " for ",
  "?",
  ":",
  "return ",
  "goto ",
  NULL
};
void
parse_chars (gchar     *text,
	     gchar    **end_ptr,
	     gint      *state,
	     gchar    **tag,
	     gboolean   start)
{
  gint i;
  gchar *next_token;

  /* Handle comments first */
  if (*state == STATE_IN_COMMENT)
    {
      *end_ptr = strstr (text, "*/");
      if (*end_ptr)
	{
	  *end_ptr += 2;
	  *state = STATE_NORMAL;
	  *tag = "comment";
	}
      return;
    }

  *tag = NULL;
  *end_ptr = NULL;

  /* check for comment */
  if (!strncmp (text, "/*", 2))
    {
      *end_ptr = strstr (text, "*/");
      if (*end_ptr)
	*end_ptr += 2;
      else
	*state = STATE_IN_COMMENT;
      *tag = "comment";
      return;
    }

  /* check for preprocessor defines */
  if (*text == '#' && start)
    {
      *end_ptr = NULL;
      *tag = "preprocessor";
      return;
    }

  /* functions */
  if (start && * text != '\t' && *text != ' ' && *text != '{' && *text != '}')
    {
      if (strstr (text, "("))
	{
	  *end_ptr = strstr (text, "(");
	  *tag = "function";
	  return;
	}
    }
  /* check for types */
  for (i = 0; types[i] != NULL; i++)
    if (!strncmp (text, types[i], strlen (types[i])))
      {
	*end_ptr = text + strlen (types[i]);
	*tag = "type";
	return;
      }

  /* check for control */
  for (i = 0; control[i] != NULL; i++)
    if (!strncmp (text, control[i], strlen (control[i])))
      {
	*end_ptr = text + strlen (control[i]);
	*tag = "control";
	return;
      }

  /* check for string */
  if (text[0] == '"')
    {
      gint maybe_escape = FALSE;

      *end_ptr = text + 1;
      *tag = "string";
      while (**end_ptr != '\000')
	{
	  if (**end_ptr == '\"' && !maybe_escape)
	    {
	      *end_ptr += 1;
	      return;
	    }
	  if (**end_ptr == '\\')
	    maybe_escape = TRUE;
	  else
	    maybe_escape = FALSE;
	  *end_ptr += 1;
	}
      return;
    }

  /* not at the start of a tag.  Find the next one. */
  for (i = 0; tokens[i] != NULL; i++)
    {
      next_token = strstr (text, tokens[i]);
      if (next_token)
	{
	  if (*end_ptr)
	    *end_ptr = (*end_ptr<next_token)?*end_ptr:next_token;
	  else
	    *end_ptr = next_token;
	}
    }

  for (i = 0; types[i] != NULL; i++)
    {
      next_token = strstr (text, types[i]);
      if (next_token)
	{
	  if (*end_ptr)
	    *end_ptr = (*end_ptr<next_token)?*end_ptr:next_token;
	  else
	    *end_ptr = next_token;
	}
    }

  for (i = 0; control[i] != NULL; i++)
    {
      next_token = strstr (text, control[i]);
      if (next_token)
	{
	  if (*end_ptr)
	    *end_ptr = (*end_ptr<next_token)?*end_ptr:next_token;
	  else
	    *end_ptr = next_token;
	}
    }
}

/* While not as cool as c-mode, this will do as a quick attempt at highlighting */
static void
fontify ()
{
  GtkTextIter start_iter, next_iter, tmp_iter;
  gint state;
  gchar *text;
  gchar *start_ptr, *end_ptr;
  gchar *tag;

  state = STATE_NORMAL;

  gtk_text_buffer_get_iter_at_offset (source_buffer, &start_iter, 0);

  next_iter = start_iter;
  while (gtk_text_iter_forward_line (&next_iter))
    {
      gboolean start = TRUE;
      start_ptr = text = gtk_text_iter_get_text (&start_iter, &next_iter);

      do
	{
	  parse_chars (start_ptr, &end_ptr, &state, &tag, start);

	  start = FALSE;
	  if (end_ptr)
	    {
	      tmp_iter = start_iter;
	      gtk_text_iter_forward_chars (&tmp_iter, end_ptr - start_ptr);
	    }
	  else
	    {
	      tmp_iter = next_iter;
	    }
	  if (tag)
	    gtk_text_buffer_apply_tag_by_name (info_buffer, tag, &start_iter, &tmp_iter);

	  start_iter = tmp_iter;
	  start_ptr = end_ptr;
	}
      while (end_ptr);

      g_free (text);
      start_iter = next_iter;
    }
}

void
load_file (const gchar *filename)
{
  FILE *file;
  GtkTextIter start, end;
  GString *buffer = g_string_new (NULL);
  int state = 0;
  gboolean in_para = 0;

  if (current_file && !strcmp (current_file, filename))
    {
      g_string_free (buffer, TRUE);
      return;
    }

  g_free (current_file);
  current_file = g_strdup (filename);
  
  gtk_text_buffer_get_bounds (info_buffer, &start, &end);
  gtk_text_buffer_delete (info_buffer, &start, &end);

  gtk_text_buffer_get_bounds (source_buffer, &start, &end);
  gtk_text_buffer_delete (source_buffer, &start, &end);

  file = fopen (filename, "r");

  if (!file)
    {
      char *installed = g_strconcat (DEMOCODEDIR,
                                     G_DIR_SEPARATOR_S,
                                     filename,
                                     NULL);

      file = fopen (installed, "r");

      g_free (installed);
    }
  
  if (!file)
    {
      g_warning ("Cannot open %s: %s\n", filename, g_strerror (errno));
      return;
    }

  gtk_text_buffer_get_iter_at_offset (info_buffer, &start, 0);
  while (read_line (file, buffer))
    {
      gchar *p = buffer->str;
      gchar *q;
      
      switch (state)
	{
	case 0:
	  /* Reading title */
	  while (*p == '/' || *p == '*' || isspace (*p))
	    p++;
	  q = p + strlen (p);
	  while (q > p && isspace (*(q - 1)))
	    q--;

	  if (q > p)
	    {
	      int len_chars = g_utf8_pointer_to_offset (p, q);

	      end = start;

	      g_assert (strlen (p) >= q - p);
	      gtk_text_buffer_insert (info_buffer, &end, p, q - p);
	      start = end;

	      gtk_text_iter_backward_chars (&start, len_chars);
	      gtk_text_buffer_apply_tag_by_name (info_buffer, "title", &start, &end);

	      start = end;
	      
	      state++;
	    }
	  break;
	    
	case 1:
	  /* Reading body of info section */
	  while (isspace (*p))
	    p++;
	  if (*p == '*' && *(p + 1) == '/')
	    {
	      gtk_text_buffer_get_iter_at_offset (source_buffer, &start, 0);
	      state++;
	    }
	  else
	    {
	      int len;
	      
	      while (*p == '*' || isspace (*p))
		p++;

	      len = strlen (p);
	      while (isspace (*(p + len - 1)))
		len--;
	      
	      if (len > 0)
		{
		  if (in_para)
		    gtk_text_buffer_insert (info_buffer, &start, " ", 1);

		  g_assert (strlen (p) >= len);
		  gtk_text_buffer_insert (info_buffer, &start, p, len);
		  in_para = 1;
		}
	      else
		{
		  gtk_text_buffer_insert (info_buffer, &start, "\n", 1);
		  in_para = 0;
		}
	    }
	  break;

	case 2:
	  /* Skipping blank lines */
	  while (isspace (*p))
	    p++;
	  if (*p)
	    {
	      p = buffer->str;
	      state++;
	      /* Fall through */
	    }
	  else
	    break;
	  
	case 3:
	  /* Reading program body */
	  gtk_text_buffer_insert (source_buffer, &start, p, -1);
	  gtk_text_buffer_insert (info_buffer, &start, "\n", 1);
	  break;
	}
    }

  fontify ();

  g_string_free (buffer, TRUE);
}

gboolean
button_press_event_cb (GtkTreeView    *tree_view,
		       GdkEventButton *event,
		       GtkTreeModel   *model)
{
  if (event->type == GDK_2BUTTON_PRESS)
    {
      GtkTreePath *path = NULL;

      gtk_tree_view_get_path_at_pos (tree_view,
				     event->window,
				     event->x,
				     event->y,
				     &path,
                                     NULL,
                                     NULL,
				     NULL);

      if (path)
	{
	  GtkTreeIter iter;
	  gboolean italic;
	  GDoDemoFunc func;
	  GtkWidget *window;

	  gtk_tree_model_get_iter (model, &iter, path);
	  gtk_tree_model_get (GTK_TREE_MODEL (model),
			      &iter,
			      FUNC_COLUMN, &func,
			      ITALIC_COLUMN, &italic,
			      -1);
	  gtk_tree_store_set (GTK_TREE_STORE (model),
			      &iter,
			      ITALIC_COLUMN, !italic,
			      -1);
	  window = (func) ();
	  if (window != NULL)
	    {
	      CallbackData *cbdata;

	      cbdata = g_new (CallbackData, 1);
	      cbdata->model = model;
	      cbdata->path = path;

	      gtk_signal_connect (GTK_OBJECT (window),
                                  "destroy",
                                  GTK_SIGNAL_FUNC (window_closed_cb),
                                  cbdata);
	    }
	  else
	    {
	      gtk_tree_path_free (path);
	    }
	}

      gtk_signal_emit_stop_by_name (GTK_OBJECT (tree_view),
				    "button_press_event");
      return TRUE;
    }
  
  return FALSE;
}

void
row_activated_cb (GtkTreeView       *tree_view,
                  GtkTreePath       *path,
		  GtkTreeViewColumn *column)
{
  GtkTreeIter iter;
  gboolean italic;
  GDoDemoFunc func;
  GtkWidget *window;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (tree_view);
  
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (model),
		      &iter,
		      FUNC_COLUMN, &func,
		      ITALIC_COLUMN, &italic,
		      -1);
  gtk_tree_store_set (GTK_TREE_STORE (model),
		      &iter,
		      ITALIC_COLUMN, !italic,
		      -1);
  window = (func) ();

  if (window != NULL)
    {
      CallbackData *cbdata;
      
      cbdata = g_new (CallbackData, 1);
      cbdata->model = model;
      cbdata->path = gtk_tree_path_copy (path);
      
      gtk_signal_connect (GTK_OBJECT (window),
			  "destroy",
			  GTK_SIGNAL_FUNC (window_closed_cb),
			  cbdata);
    }
}

static void
selection_cb (GtkTreeSelection *selection,
	      GtkTreeModel     *model)
{
  GtkTreeIter iter;
  GValue value = {0, };

  if (! gtk_tree_selection_get_selected (selection, NULL, &iter))
    return;

  gtk_tree_model_get_value (model, &iter,
			    FILENAME_COLUMN,
			    &value);
  load_file (g_value_get_string (&value));
  g_value_unset (&value);
}

static GtkWidget *
create_text (GtkTextBuffer **buffer,
	     gboolean        is_source)
{
  GtkWidget *scrolled_window;
  GtkWidget *text_view;
  PangoFontDescription *font_desc;

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
				       GTK_SHADOW_IN);
  
  text_view = gtk_text_view_new ();
  gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
  
  *buffer = gtk_text_buffer_new (NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (text_view), *buffer);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);

  if (is_source)
    {
      font_desc = pango_font_description_from_string ("Courier 12");
      gtk_widget_modify_font (text_view, font_desc);
      pango_font_description_free (font_desc);

      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view),
                                   GTK_WRAP_NONE);
    }
  else
    {
      /* Make it a bit nicer for text. */
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view),
                                   GTK_WRAP_WORD);
      gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (text_view),
                                            2);
      gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (text_view),
                                            2);
    }
  
  return scrolled_window;
}

/* Technically a list, but if we do go to 80 demos, we may want to move to a tree */
static GtkWidget *
create_tree (void)
{
  GtkTreeSelection *selection;
  GtkCellRenderer *cell;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkTreeStore *model;
  GtkTreeIter iter;
  gint i;

  model = gtk_tree_store_new_with_types (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_BOOLEAN);
  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

  gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			       GTK_TREE_SELECTION_SINGLE);
  gtk_widget_set_usize (tree_view, 200, -1);

  for (i=0; i < G_N_ELEMENTS (testgtk_demos); i++)
    {
      gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

      gtk_tree_store_set (GTK_TREE_STORE (model),
			  &iter,
			  TITLE_COLUMN, testgtk_demos[i].title,
			  FILENAME_COLUMN, testgtk_demos[i].filename,
			  FUNC_COLUMN, testgtk_demos[i].func,
			  ITALIC_COLUMN, FALSE,
			  -1);
    }

  cell = gtk_cell_renderer_text_new ();

  g_object_set (G_OBJECT (cell),
                "style", PANGO_STYLE_ITALIC,
                NULL);
  
  column = gtk_tree_view_column_new_with_attributes ("Widget (double click for demo)",
						     cell,
						     "text", TITLE_COLUMN,
						     "style_set", ITALIC_COLUMN,
						     NULL);
  
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
			       GTK_TREE_VIEW_COLUMN (column));

  g_signal_connectc (G_OBJECT (selection), "changed", GTK_SIGNAL_FUNC (selection_cb), model, FALSE);
  gtk_signal_connect (GTK_OBJECT (tree_view), "row_activated", GTK_SIGNAL_FUNC (row_activated_cb), model);

  return tree_view;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *notebook;
  GtkWidget *hbox;
  GtkWidget *tree;
  GtkTextTag *tag;

  /* Most code in gtk-demo is intended to be exemplary, but not
   * these few lines, which are just a hack so gtk-demo will work
   * in the GTK tree without installing it.
   */
  if (g_file_test ("../../gdk-pixbuf/.libs/libpixbufloader-pnm.so",
                   G_FILE_TEST_EXISTS))
    {
      putenv ("GDK_PIXBUF_MODULEDIR=../../gdk-pixbuf/.libs");
      putenv ("GTK_IM_MODULE_FILE=../../modules/input/gtk.immodules");
    }
  /* -- End of hack -- */
  
  gtk_init (&argc, &argv);
  
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GTK+ Code Demos");
  gtk_signal_connect (GTK_OBJECT (window), "destroy",
		      GTK_SIGNAL_FUNC (gtk_main_quit), NULL);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), hbox);

  tree = create_tree ();
  gtk_box_pack_start (GTK_BOX (hbox), tree, FALSE, FALSE, 0);

  notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (hbox), notebook, TRUE, TRUE, 0);

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    create_text (&info_buffer, FALSE),
			    gtk_label_new_with_mnemonic ("_Info"));

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
			    create_text (&source_buffer, TRUE),
			    gtk_label_new_with_mnemonic ("_Source"));

  tag = gtk_text_buffer_create_tag (info_buffer, "title",
                                    "font", "Sans 18",
                                    NULL);

  tag = gtk_text_buffer_create_tag (info_buffer, "comment",
				    "foreground", "blue",
                                    NULL);
  tag = gtk_text_buffer_create_tag (info_buffer, "type",
				    "foreground", "red",
                                    NULL);
  tag = gtk_text_buffer_create_tag (info_buffer, "string",
				    "foreground", "SpringGreen3",
				    "weight", PANGO_WEIGHT_BOLD,
                                    NULL);
  tag = gtk_text_buffer_create_tag (info_buffer, "control",
				    "foreground", "purple",
                                    NULL);
  tag = gtk_text_buffer_create_tag (info_buffer, "preprocessor",
				    "style", PANGO_STYLE_OBLIQUE,
 				    "foreground", "burlywood4",
                                    NULL);
  tag = gtk_text_buffer_create_tag (info_buffer, "function",
				    "weight", PANGO_WEIGHT_BOLD,
 				    "foreground", "DarkGoldenrod4",
                                    NULL);

  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
  gtk_widget_show_all (window);
  

  load_file (testgtk_demos[0].filename);
  
  gtk_main ();

  return 0;
}
