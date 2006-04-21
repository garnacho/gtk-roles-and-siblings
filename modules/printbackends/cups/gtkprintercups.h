/* GtkPrinterCups
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
#ifndef __GTK_PRINTER_CUPS_H__
#define __GTK_PRINTER_CUPS_H__

#include <glib.h>
#include <glib-object.h>
#include <cups/cups.h>
#include <cups/ppd.h>

#include "gtkprinter.h"

G_BEGIN_DECLS

#define GTK_TYPE_PRINTER_CUPS                  (gtk_printer_cups_get_type ())
#define GTK_PRINTER_CUPS(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PRINTER_CUPS, GtkPrinterCups))
#define GTK_PRINTER_CUPS_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PRINTER_CUPS, GtkPrinterCupsClass))
#define GTK_IS_PRINTER_CUPS(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PRINTER_CUPS))
#define GTK_IS_PRINTER_CUPS_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PRINTER_CUPS))
#define GTK_PRINTER_CUPS_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PRINTER_CUPS, GtkPrinterCupsClass))

typedef struct _GtkPrinterCups	        GtkPrinterCups;
typedef struct _GtkPrinterCupsClass     GtkPrinterCupsClass;
typedef struct _GtkPrinterCupsPrivate   GtkPrinterCupsPrivate;

struct _GtkPrinterCups
{
  GtkPrinter parent_instance;

  gchar *device_uri;
  gchar *printer_uri;
  gchar *hostname;
  gint port;

  ipp_pstate_t state;
  gboolean reading_ppd;
  ppd_file_t *ppd_file;
};

struct _GtkPrinterCupsClass
{
  GtkPrinterClass parent_class;

};

GType                    gtk_printer_cups_get_type      (void) G_GNUC_CONST;
void                     gtk_printer_cups_register_type (GTypeModule     *module);
GtkPrinterCups          *gtk_printer_cups_new           (const char      *name,
							 GtkPrintBackend *backend);
ppd_file_t *             gtk_printer_cups_get_ppd       (GtkPrinterCups  *printer);

G_END_DECLS

#endif /* __GTK_PRINTER_CUPS_H__ */
