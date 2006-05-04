/* gtkcupsutils.h 
 * Copyright (C) 2006 John (J5) Palmieri <johnp@redhat.com>
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
 
#ifndef __GTK_CUPS_UTILS_H__
#define __GTK_CUPS_UTILS_H__

#include <glib.h>
#include <cups/cups.h>
#include <cups/language.h>
#include <cups/http.h>
#include <cups/ipp.h>

G_BEGIN_DECLS

typedef struct _GtkCupsRequest  GtkCupsRequest;
typedef struct _GtkCupsResult   GtkCupsResult;

typedef enum
{
  GTK_CUPS_POST,
  GTK_CUPS_GET
} GtkCupsRequestType;


/** 
 * Direction we should be polling the http socket on.
 * We are either reading or writting at each state.
 * This makes it easy for mainloops to connect to poll.
 */
typedef enum
{
  GTK_CUPS_HTTP_IDLE,
  GTK_CUPS_HTTP_READ,
  GTK_CUPS_HTTP_WRITE
} GtkCupsPollState;


struct _GtkCupsRequest 
{
  GtkCupsRequestType type;

  http_t *http;
  http_status_t last_status;
  ipp_t *ipp_request;

  gchar *server;
  gchar *resource;
  gint data_fd;
  gint attempts;

  GtkCupsResult *result;

  gint state;
  GtkCupsPollState poll_state;

  gint own_http : 1; 
};

#define GTK_CUPS_REQUEST_START 0
#define GTK_CUPS_REQUEST_DONE 500

/* POST states */
enum 
{
  GTK_CUPS_POST_CONNECT = GTK_CUPS_REQUEST_START,
  GTK_CUPS_POST_SEND,
  GTK_CUPS_POST_WRITE_REQUEST,
  GTK_CUPS_POST_WRITE_DATA,
  GTK_CUPS_POST_CHECK,
  GTK_CUPS_POST_READ_RESPONSE,
  GTK_CUPS_POST_DONE = GTK_CUPS_REQUEST_DONE
};

/* GET states */
enum
{
  GTK_CUPS_GET_CONNECT = GTK_CUPS_REQUEST_START,
  GTK_CUPS_GET_SEND,
  GTK_CUPS_GET_CHECK,
  GTK_CUPS_GET_READ_DATA,
  GTK_CUPS_GET_DONE = GTK_CUPS_REQUEST_DONE
};

GtkCupsRequest * gtk_cups_request_new             (http_t             *connection,
						   GtkCupsRequestType  req_type,
						   gint                operation_id,
						   gint                data_fd,
						   const char         *server,
						   const char         *resource);
void             gtk_cups_request_ipp_add_string  (GtkCupsRequest     *request,
						   ipp_tag_t           group,
						   ipp_tag_t           tag,
						   const char         *name,
						   const char         *charset,
						   const char         *value);
void             gtk_cups_request_ipp_add_strings (GtkCupsRequest     *request,
						   ipp_tag_t           group,
						   ipp_tag_t           tag,
						   const char         *name,
						   int                 num_values,
						   const char         *charset,
						   const char * const *values);
gboolean         gtk_cups_request_read_write      (GtkCupsRequest     *request);
GtkCupsPollState gtk_cups_request_get_poll_state  (GtkCupsRequest     *request);
void             gtk_cups_request_free            (GtkCupsRequest     *request);
GtkCupsResult  * gtk_cups_request_get_result      (GtkCupsRequest     *request);
gboolean         gtk_cups_request_is_done         (GtkCupsRequest     *request);
void             gtk_cups_request_encode_option   (GtkCupsRequest     *request,
						   const gchar        *option,
						   const gchar        *value);
gboolean         gtk_cups_result_is_error         (GtkCupsResult      *result);
ipp_t          * gtk_cups_result_get_response     (GtkCupsResult      *result);
const char     * gtk_cups_result_get_error_string (GtkCupsResult      *result);

G_END_DECLS
#endif 
