/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __GTK_SIGNAL_H__
#define __GTK_SIGNAL_H__


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkobject.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#ifdef offsetof
#define GTK_SIGNAL_OFFSET(t, f) ((int) offsetof (t, f))
#else /* offsetof */
#define GTK_SIGNAL_OFFSET(t, f) ((int) ((char*) &((t*) 0)->f))
#endif /* offsetof */

  
typedef void (*GtkSignalMarshal)    (GtkObject      *object,
				     gpointer        data,
				     gint            nparams,
				     GtkArg         *args,
				     GtkType        *arg_types,
				     GtkType         return_type);
typedef void (*GtkSignalDestroy)    (gpointer        data);

typedef struct _GtkSignalQuery		GtkSignalQuery;

struct	_GtkSignalQuery
{
  gint		   object_type;
  const gchar	  *signal_name;
  gboolean	   is_user_signal;
  GtkSignalRunType run_type;
  GtkType	   return_val;
  guint		   nparams;
  const GtkType	  *params;
};

gint   gtk_signal_new                     (const gchar         *name,
					   GtkSignalRunType     run_type,
					   gint                 object_type,
					   gint                 function_offset,
					   GtkSignalMarshaller  marshaller,
					   GtkType              return_val,
					   gint                 nparams,
					   ...);
gint   gtk_signal_newv                    (const gchar         *name,
					   GtkSignalRunType     run_type,
					   gint                 object_type,
					   gint                 function_offset,
					   GtkSignalMarshaller  marshaller,
					   GtkType              return_val,
					   gint                 nparams,
					   GtkType	       *params);
gint   gtk_signal_lookup                  (const gchar         *name,
					   gint                 object_type);
gchar* gtk_signal_name                    (gint                 signal_num);
void   gtk_signal_emit                    (GtkObject           *object,
					   gint                 signal_type,
					   ...);
void   gtk_signal_emit_by_name            (GtkObject           *object,
					   const gchar         *name,
					   ...);
void   gtk_signal_emit_stop               (GtkObject           *object,
					   gint                 signal_type);
void   gtk_signal_emit_stop_by_name       (GtkObject           *object,
					   const gchar         *name);
gint   gtk_signal_connect                 (GtkObject           *object,
					   const gchar         *name,
					   GtkSignalFunc        func,
					   gpointer             func_data);
gint   gtk_signal_connect_after           (GtkObject           *object,
					   const gchar         *name,
					   GtkSignalFunc        func,
					   gpointer             func_data);
gint   gtk_signal_connect_object          (GtkObject           *object,
					   const gchar         *name,
					   GtkSignalFunc        func,
					   GtkObject           *slot_object);
gint   gtk_signal_connect_object_after    (GtkObject           *object,
					   const gchar         *name,
					   GtkSignalFunc        func,
					   GtkObject           *slot_object);
gint   gtk_signal_connect_interp          (GtkObject           *object,
					   gchar               *name,
					   GtkCallbackMarshal   func,
					   gpointer             data,
					   GtkDestroyNotify     destroy_func,
					   gint                 after);

void   gtk_signal_connect_object_while_alive (GtkObject        *object,
					      const gchar      *signal,
					      GtkSignalFunc     func,
					      GtkObject        *alive_object);
void   gtk_signal_connect_while_alive	     (GtkObject        *object,
					      const gchar      *signal,
					      GtkSignalFunc     func,
					      gpointer          func_data,
					      GtkObject        *alive_object);

void   gtk_signal_disconnect              (GtkObject           *object,
					   gint                 anid);
void   gtk_signal_disconnect_by_data      (GtkObject           *object,
					   gpointer             data);
void   gtk_signal_handler_block           (GtkObject           *object,
					   gint                 anid);
void   gtk_signal_handler_block_by_data   (GtkObject           *object,
					   gpointer             data);
void   gtk_signal_handler_unblock         (GtkObject           *object,
					   gint                 anid);
void   gtk_signal_handler_unblock_by_data (GtkObject           *object,
					   gpointer             data);
guint  gtk_signal_handler_pending	  (GtkObject	       *object,
					   gint		        signal_type,
					   gboolean		may_be_blocked);
void   gtk_signal_handlers_destroy        (GtkObject           *object);
void   gtk_signal_default_marshaller      (GtkObject           *object,
					   GtkSignalFunc        func,
					   gpointer             func_data,
					   GtkArg              *args);
void   gtk_signal_set_funcs               (GtkSignalMarshal     marshal_func,
					   GtkSignalDestroy     destroy_func);

/* Report internal information about a signal. The caller has the response
 *  to invoke a supsequent g_free (returned_data); but must leave the
 *  contents of GtkSignalQuery untouched.
 */
GtkSignalQuery* gtk_signal_query	  (gint			signal_num);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SIGNAL_H__ */
