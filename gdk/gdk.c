/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "gdk.h"
#include "gdkinternals.h"

#ifndef HAVE_XCONVERTCASE
#include "gdkkeysyms.h"
#endif

typedef struct _GdkPredicate  GdkPredicate;
typedef struct _GdkErrorTrap  GdkErrorTrap;

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

struct _GdkErrorTrap
{
  gint error_warnings;
  gint error_code;
};

/* 
 * Private function declarations
 */

GdkFilterReturn gdk_wm_protocols_filter (GdkXEvent *xev,
					 GdkEvent  *event,
					 gpointer   data);

/* Private variable declarations
 */
static int gdk_initialized = 0;			    /* 1 if the library is initialized,
						     * 0 otherwise.
						     */

static GSList *gdk_error_traps = NULL;               /* List of error traps */
static GSList *gdk_error_trap_free_list = NULL;      /* Free list */

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  {"events",	    GDK_DEBUG_EVENTS},
  {"misc",	    GDK_DEBUG_MISC},
  {"dnd",	    GDK_DEBUG_DND},
  {"color-context", GDK_DEBUG_COLOR_CONTEXT},
  {"xim",	    GDK_DEBUG_XIM}
};

static const int gdk_ndebug_keys = sizeof(gdk_debug_keys)/sizeof(GDebugKey);

#endif /* G_ENABLE_DEBUG */

GdkArgContext *
gdk_arg_context_new (gpointer cb_data)
{
  GdkArgContext *result = g_new (GdkArgContext, 1);
  result->tables = g_ptr_array_new ();
  result->cb_data = cb_data;

  return result;
}

void
gdk_arg_context_destroy (GdkArgContext *context)
{
  g_ptr_array_free (context->tables, TRUE);
  g_free (context);
}

void
gdk_arg_context_add_table (GdkArgContext *context, GdkArgDesc *table)
{
  g_ptr_array_add (context->tables, table);
}

void
gdk_arg_context_parse (GdkArgContext *context, gint *argc, gchar ***argv)
{
  int i, j, k;

  /* Save a copy of the original argc and argv */
  if (argc && argv)
    {
      for (i = 1; i < *argc; i++)
	{
	  char *arg;
	  
	  if (!(*argv)[i][0] == '-' && (*argv)[i][1] == '-')
	    continue;
	  
	  arg = (*argv)[i] + 2;

	  /* '--' terminates list of arguments */
	  if (arg == 0)
	    {
	      (*argv)[i] = NULL;
	      break;
	    }
	      
	  for (j = 0; j < context->tables->len; j++)
	    {
	      GdkArgDesc *table = context->tables->pdata[j];
	      for (k = 0; table[k].name; k++)
		{
		  switch (table[k].type)
		    {
		    case GDK_ARG_STRING:
		    case GDK_ARG_CALLBACK:
		    case GDK_ARG_INT:
		      {
			int len = strlen (table[k].name);
			
			if (strncmp (arg, table[k].name, len) == 0 &&
			    (arg[len] == '=' || argc[len] == 0))
			  {
			    char *value = NULL;

			    (*argv)[i] = NULL;

			    if (arg[len] == '=')
			      value = arg + len + 1;
			    else if (i < *argc - 1)
			      {
				value = (*argv)[i + 1];
				(*argv)[i+1] = NULL;
				i++;
			      }
			    else
			      value = "";

			    switch (table[k].type)
			      {
			      case GDK_ARG_STRING:
				*(gchar **)table[k].location = g_strdup (value);
				break;
			      case GDK_ARG_INT:
				*(gint *)table[k].location = atoi (value);
				break;
			      case GDK_ARG_CALLBACK:
				(*table[k].callback)(table[k].name, value, context->cb_data);
				break;
			      default:
				;
			      }

			    goto next_arg;
			  }
			break;
		      }
		    case GDK_ARG_BOOL:
		    case GDK_ARG_NOBOOL:
		      if (strcmp (arg, table[k].name) == 0)
			{
			  (*argv)[i] = NULL;
			  
			  *(gboolean *)table[k].location = (table[k].type == GDK_ARG_BOOL) ? TRUE : FALSE;
			  goto next_arg;
			}
		    }
		}
	    }
	next_arg:
	  ;
	}
	  
      for (i = 1; i < *argc; i++)
	{
	  for (k = i; k < *argc; k++)
	    if ((*argv)[k] != NULL)
	      break;
	  
	  if (k > i)
	    {
	      k -= i;
	      for (j = i + k; j < *argc; j++)
		(*argv)[j-k] = (*argv)[j];
	      *argc -= k;
	    }
	}
    }
}

#ifdef G_ENABLE_DEBUG
static void
gdk_arg_debug_cb (const char *key, const char *value, gpointer user_data)
{
  gdk_debug_flags |= g_parse_debug_string (value,
					   (GDebugKey *) gdk_debug_keys,
					   gdk_ndebug_keys);
}

static void
gdk_arg_no_debug_cb (const char *key, const char *value, gpointer user_data)
{
  gdk_debug_flags &= ~g_parse_debug_string (value,
					    (GDebugKey *) gdk_debug_keys,
					    gdk_ndebug_keys);
}
#endif /* G_ENABLE_DEBUG */

static void
gdk_arg_name_cb (const char *key, const char *value, gpointer user_data)
{
  g_set_prgname (value);
}

static GdkArgDesc gdk_args[] = {
  { "name",         GDK_ARG_STRING,   NULL, gdk_arg_name_cb     },
#ifdef G_ENABLE_DEBUG
  { "gdk-debug",    GDK_ARG_CALLBACK, NULL, gdk_arg_debug_cb    },
  { "gdk-no-debug", GDK_ARG_CALLBACK, NULL, gdk_arg_no_debug_cb },
#endif /* G_ENABLE_DEBUG */
  { NULL }
};

/*
 *--------------------------------------------------------------
 * gdk_init_check
 *
 *   Initialize the library for use.
 *
 * Arguments:
 *   "argc" is the number of arguments.
 *   "argv" is an array of strings.
 *
 * Results:
 *   "argc" and "argv" are modified to reflect any arguments
 *   which were not handled. (Such arguments should either
 *   be handled by the application or dismissed). If initialization
 *   fails, returns FALSE, otherwise TRUE.
 *
 * Side effects:
 *   The library is initialized.
 *
 *--------------------------------------------------------------
 */

gboolean
gdk_init_check (int    *argc,
		char ***argv)
{
  gchar **argv_orig = NULL;
  gint argc_orig = 0;
  GdkArgContext *arg_context;
  gboolean result;
  int i;
  
  if (gdk_initialized)
    return TRUE;

  if (g_thread_supported ())
    gdk_threads_mutex = g_mutex_new ();
  
  if (argc && argv)
    {
      argc_orig = *argc;
      
      argv_orig = g_malloc ((argc_orig + 1) * sizeof (char*));
      for (i = 0; i < argc_orig; i++)
	argv_orig[i] = g_strdup ((*argv)[i]);
      argv_orig[argc_orig] = NULL;

      if (*argc > 0)
	{
	  gchar *d;
	  
	  d = strrchr((*argv)[0], G_DIR_SEPARATOR);
	  if (d != NULL)
	    g_set_prgname (d + 1);
	  else
	    g_set_prgname ((*argv)[0]);
	}
    }

  
#ifdef G_ENABLE_DEBUG
  {
    gchar *debug_string = getenv("GDK_DEBUG");
    if (debug_string != NULL)
      gdk_debug_flags = g_parse_debug_string (debug_string,
					      (GDebugKey *) gdk_debug_keys,
					      gdk_ndebug_keys);
  }
#endif	/* G_ENABLE_DEBUG */
  
  arg_context = gdk_arg_context_new (NULL);
  gdk_arg_context_add_table (arg_context, gdk_args);
  gdk_arg_context_add_table (arg_context, _gdk_windowing_args);
  gdk_arg_context_parse (arg_context, argc, argv);
  gdk_arg_context_destroy (arg_context);
  
  GDK_NOTE (MISC, g_message ("progname: \"%s\"", g_get_prgname ()));

  g_type_init (0);
  
  result = _gdk_windowing_init_check (argc_orig, argv_orig);

  for (i = 0; i < argc_orig; i++)
    g_free(argv_orig[i]);
  g_free(argv_orig);

  if (!result)
    return FALSE;
  
  gdk_events_init ();
  gdk_visual_init ();
  _gdk_windowing_window_init ();
  _gdk_windowing_image_init ();
  gdk_input_init ();
  gdk_dnd_init ();

  gdk_initialized = 1;

  return TRUE;
}

void
gdk_init (int *argc, char ***argv)
{
  if (!gdk_init_check (argc, argv))
    {
      g_warning ("cannot open display: %s", gdk_get_display ());
      exit(1);
    }
}

/*
 *--------------------------------------------------------------
 * gdk_exit
 *
 *   Restores the library to an un-itialized state and exits
 *   the program using the "exit" system call.
 *
 * Arguments:
 *   "errorcode" is the error value to pass to "exit".
 *
 * Results:
 *   Allocated structures are freed and the program exits
 *   cleanly.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_exit (gint errorcode)
{
  /* de-initialisation is done by the gdk_exit_funct(),
     no need to do this here (Alex J.) */
  exit (errorcode);
}

#if 0

/* This is disabled, but the code isn't removed, because we might
 * want to have some sort of explicit way to shut down GDK cleanly
 * at some point in the future.
 */

/*
 *--------------------------------------------------------------
 * gdk_exit_func
 *
 *   This is the "atexit" function that makes sure the
 *   library gets a chance to cleanup.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *   The library is un-initialized and the program exits.
 *
 *--------------------------------------------------------------
 */

static void
gdk_exit_func (void)
{
  static gboolean in_gdk_exit_func = FALSE;
  
  /* This is to avoid an infinite loop if a program segfaults in
     an atexit() handler (and yes, it does happen, especially if a program
     has trounced over memory too badly for even g_message to work) */
  if (in_gdk_exit_func == TRUE)
    return;
  in_gdk_exit_func = TRUE;
  
  if (gdk_initialized)
    {
      gdk_image_exit ();
      gdk_input_exit ();
      gdk_key_repeat_restore ();

      gdk_windowing_exit ();
      
      gdk_initialized = 0;
    }
}

#endif

/*************************************************************
 * gdk_error_trap_push:
 *     Push an error trap. X errors will be trapped until
 *     the corresponding gdk_error_pop(), which will return
 *     the error code, if any.
 *   arguments:
 *     
 *   results:
 *************************************************************/

void
gdk_error_trap_push (void)
{
  GSList *node;
  GdkErrorTrap *trap;

  if (gdk_error_trap_free_list)
    {
      node = gdk_error_trap_free_list;
      gdk_error_trap_free_list = gdk_error_trap_free_list->next;
    }
  else
    {
      node = g_slist_alloc ();
      node->data = g_new (GdkErrorTrap, 1);
    }

  node->next = gdk_error_traps;
  gdk_error_traps = node;
  
  trap = node->data;
  trap->error_code = gdk_error_code;
  trap->error_warnings = gdk_error_warnings;

  gdk_error_code = 0;
  gdk_error_warnings = 0;
}

/*************************************************************
 * gdk_error_trap_pop:
 *     Pop an error trap added with gdk_error_push()
 *   arguments:
 *     
 *   results:
 *     0, if no error occured, otherwise the error code.
 *************************************************************/

gint
gdk_error_trap_pop (void)
{
  GSList *node;
  GdkErrorTrap *trap;
  gint result;

  g_return_val_if_fail (gdk_error_traps != NULL, 0);

  node = gdk_error_traps;
  gdk_error_traps = gdk_error_traps->next;

  node->next = gdk_error_trap_free_list;
  gdk_error_trap_free_list = node;
  
  result = gdk_error_code;
  
  trap = node->data;
  gdk_error_code = trap->error_code;
  gdk_error_warnings = trap->error_warnings;
  
  return result;
}

void
gdk_threads_enter ()
{
  GDK_THREADS_ENTER ();
}

void
gdk_threads_leave ()
{
  GDK_THREADS_LEAVE ();
}

