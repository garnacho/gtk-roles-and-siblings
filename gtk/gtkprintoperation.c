/* GTK - The GIMP Toolkit
 * gtkprintoperation.c: Print Operation
 * Copyright (C) 2006, Red Hat, Inc.
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

#include "config.h"
#include <string.h>
#include "gtkprintoperation-private.h"
#include "gtkmarshalers.h"
#include <cairo-pdf.h>
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkmessagedialog.h"
#include "gtkalias.h"

#define SHOW_PROGRESS_TIME 1200

#define GTK_PRINT_OPERATION_GET_PRIVATE(obj)(G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_PRINT_OPERATION, GtkPrintOperationPrivate))

enum {
  BEGIN_PRINT,
  PAGINATE,
  REQUEST_PAGE_SETUP,
  DRAW_PAGE,
  END_PRINT,
  STATUS_CHANGED,
  CREATE_CUSTOM_WIDGET,
  CUSTOM_WIDGET_APPLY,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_DEFAULT_PAGE_SETUP,
  PROP_PRINT_SETTINGS,
  PROP_JOB_NAME,
  PROP_N_PAGES,
  PROP_CURRENT_PAGE,
  PROP_USE_FULL_PAGE,
  PROP_TRACK_PRINT_STATUS,
  PROP_UNIT,
  PROP_SHOW_DIALOG,
  PROP_SHOW_PROGRESS,
  PROP_PDF_TARGET,
  PROP_STATUS,
  PROP_STATUS_STRING,
  PROP_CUSTOM_TAB_LABEL
};

static guint signals[LAST_SIGNAL] = { 0 };
static int job_nr = 0;

G_DEFINE_TYPE (GtkPrintOperation, gtk_print_operation, G_TYPE_OBJECT)

/**
 * gtk_print_error_quark:
 *
 * Registers an error quark for #GtkPrintOperation if necessary.
 * 
 * Return value: The error quark used for #GtkPrintOperation errors.
 *
 * Since: 2.10
 **/
GQuark     
gtk_print_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("gtk-print-error-quark");
  return quark;
}
     
static void
gtk_print_operation_finalize (GObject *object)
{
  GtkPrintOperation *print_operation = GTK_PRINT_OPERATION (object);
  GtkPrintOperationPrivate *priv = print_operation->priv;

  if (priv->free_platform_data &&
      priv->platform_data)
    {
      priv->free_platform_data (priv->platform_data);
      priv->free_platform_data = NULL;
    }

  if (priv->default_page_setup)
    g_object_unref (priv->default_page_setup);
  
  if (priv->print_settings)
    g_object_unref (priv->print_settings);
  
  g_free (priv->pdf_target);
  g_free (priv->job_name);
  g_free (priv->custom_tab_label);

  if (priv->print_pages_idle_id > 0)
    g_source_remove (priv->print_pages_idle_id);

  if (priv->show_progress_timeout_id > 0)
    g_source_remove (priv->show_progress_timeout_id);

  G_OBJECT_CLASS (gtk_print_operation_parent_class)->finalize (object);
}

static void
gtk_print_operation_init (GtkPrintOperation *operation)
{
  GtkPrintOperationPrivate *priv;
  const char *appname;

  priv = operation->priv = GTK_PRINT_OPERATION_GET_PRIVATE (operation);

  priv->status = GTK_PRINT_STATUS_INITIAL;
  priv->status_string = g_strdup ("");
  priv->default_page_setup = NULL;
  priv->print_settings = NULL;
  priv->nr_of_pages = -1;
  priv->current_page = -1;
  priv->use_full_page = FALSE;
  priv->show_dialog = TRUE;
  priv->show_progress = FALSE;
  priv->pdf_target = NULL;
  priv->track_print_status = FALSE;

  priv->unit = GTK_UNIT_PIXEL;

  appname = g_get_application_name ();
  priv->job_name = g_strdup_printf ("%s job #%d", appname, ++job_nr);
}

static void
gtk_print_operation_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GtkPrintOperation *op = GTK_PRINT_OPERATION (object);
  
  switch (prop_id)
    {
    case PROP_DEFAULT_PAGE_SETUP:
      gtk_print_operation_set_default_page_setup (op, g_value_get_object (value));
      break;
    case PROP_PRINT_SETTINGS:
      gtk_print_operation_set_print_settings (op, g_value_get_object (value));
      break;
    case PROP_JOB_NAME:
      gtk_print_operation_set_job_name (op, g_value_get_string (value));
      break;
    case PROP_N_PAGES:
      gtk_print_operation_set_n_pages (op, g_value_get_int (value));
      break;
    case PROP_CURRENT_PAGE:
      gtk_print_operation_set_current_page (op, g_value_get_int (value));
      break;
    case PROP_USE_FULL_PAGE:
      gtk_print_operation_set_use_full_page (op, g_value_get_boolean (value));
      break;
    case PROP_TRACK_PRINT_STATUS:
      gtk_print_operation_set_track_print_status (op, g_value_get_boolean (value));
      break;
    case PROP_UNIT:
      gtk_print_operation_set_unit (op, g_value_get_enum (value));
      break;
    case PROP_SHOW_DIALOG:
      gtk_print_operation_set_show_dialog (op, g_value_get_boolean (value));
      break;
    case PROP_SHOW_PROGRESS:
      gtk_print_operation_set_show_progress (op, g_value_get_boolean (value));
      break;
    case PROP_PDF_TARGET:
      gtk_print_operation_set_pdf_target (op, g_value_get_string (value));
      break;
    case PROP_CUSTOM_TAB_LABEL:
      gtk_print_operation_set_custom_tab_label (op, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_print_operation_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  GtkPrintOperation *op = GTK_PRINT_OPERATION (object);
  GtkPrintOperationPrivate *priv = op->priv;

  switch (prop_id)
    {
    case PROP_DEFAULT_PAGE_SETUP:
      g_value_set_object (value, priv->default_page_setup);
      break;
    case PROP_PRINT_SETTINGS:
      g_value_set_object (value, priv->print_settings);
      break;
    case PROP_JOB_NAME:
      g_value_set_string (value, priv->job_name);
      break;
    case PROP_N_PAGES:
      g_value_set_int (value, priv->nr_of_pages);
      break;
    case PROP_CURRENT_PAGE:
      g_value_set_int (value, priv->current_page);
      break;      
    case PROP_USE_FULL_PAGE:
      g_value_set_boolean (value, priv->use_full_page);
      break;
    case PROP_TRACK_PRINT_STATUS:
      g_value_set_boolean (value, priv->track_print_status);
      break;
    case PROP_UNIT:
      g_value_set_enum (value, priv->unit);
      break;
    case PROP_SHOW_DIALOG:
      g_value_set_boolean (value, priv->show_dialog);
      break;
    case PROP_SHOW_PROGRESS:
      g_value_set_boolean (value, priv->show_progress);
      break;
    case PROP_PDF_TARGET:
      g_value_set_string (value, priv->pdf_target);
      break;
    case PROP_STATUS:
      g_value_set_enum (value, priv->status);
      break;
    case PROP_STATUS_STRING:
      g_value_set_string (value, priv->status_string);
      break;
    case PROP_CUSTOM_TAB_LABEL:
      g_value_set_string (value, priv->custom_tab_label);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkWidget *
gtk_print_operation_create_custom_widget (GtkPrintOperation *operation)
{
  return NULL;
}

static gboolean
custom_widget_accumulator (GSignalInvocationHint *ihint,
			   GValue                *return_accu,
			   const GValue          *handler_return,
			   gpointer               dummy)
{
  gboolean continue_emission;
  GtkWidget *widget;
  
  widget = g_value_get_pointer (handler_return);
  if (widget != NULL)
    g_value_set_pointer (return_accu, widget);
  continue_emission = (widget == NULL);
  
  return continue_emission;
}

static void
gtk_print_operation_class_init (GtkPrintOperationClass *class)
{
  GObjectClass *gobject_class = (GObjectClass *)class;

  gobject_class->set_property = gtk_print_operation_set_property;
  gobject_class->get_property = gtk_print_operation_get_property;
  gobject_class->finalize = gtk_print_operation_finalize;

  class->create_custom_widget = gtk_print_operation_create_custom_widget;
  
  g_type_class_add_private (gobject_class, sizeof (GtkPrintOperationPrivate));

  /**
   * GtkPrintOperation::begin-print:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   *
   * Gets emitted after the user has finished changing print settings
   * in the dialog, before the actual rendering starts. 
   *
   * A typical use for this signal is to use the parameters from the
   * #GtkPrintContext and paginate the document accordingly, and then
   * set the number of pages with gtk_print_operation_set_n_pages().
   *
   * Since: 2.10
   */
  signals[BEGIN_PRINT] =
    g_signal_new (I_("begin-print"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, begin_print),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINT_CONTEXT);

   /**
   * Gtkprintoperation::paginate:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   *
   * Gets emitted after the begin-print signal, but before the actual 
   * rendering starts. It keeps getting emitted until it returns %FALSE. 
   *
   * This signal is intended to be used for paginating the document
   * in small chunks, to avoid blocking the user interface for a long
   * time. The signal handler should update the number of pages using
   * gtk_print_operation_set_n_pages(), and return %TRUE if the document
   * has been completely paginated.
   *
   * If you don't need to do pagination in chunks, you can simply do
   * it all in the begin-print handler, and set the number of pages
   * from there.
   *
   * Since: 2.10
   */
  signals[PAGINATE] =
    g_signal_new (I_("paginate"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, paginate),
		  _gtk_boolean_handled_accumulator, NULL,
		  _gtk_marshal_BOOLEAN__OBJECT,
		  G_TYPE_BOOLEAN, 1, GTK_TYPE_PRINT_CONTEXT);


  /**
   * GtkPrintOperation::request-page-setup:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   * @page_nr: the number of the currently printed page
   * @setup: the #GtkPageSetup 
   * 
   * Gets emitted once for every page that is printed, to give
   * the application a chance to modify the page setup. Any changes 
   * done to @setup will be in force only for printing this page.
   *
   * Since: 2.10
   */
  signals[REQUEST_PAGE_SETUP] =
    g_signal_new (I_("request-page-setup"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, request_page_setup),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_INT_OBJECT,
		  G_TYPE_NONE, 3,
		  GTK_TYPE_PRINT_CONTEXT,
		  G_TYPE_INT,
		  GTK_TYPE_PAGE_SETUP);

  /**
   * GtkPrintOperation::draw-page:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   * @page_nr: the number of the currently printed page
   *
   * Gets emitted for every page that is printed. The signal handler
   * must render the @page_nr's page onto the cairo context obtained
   * from @context using gtk_print_context_get_cairo().
   *
   * <informalexample><programlisting>
   *  FIXME: need an example here
   * </programlisting></informalexample>
   *
   * Use gtk_print_operation_set_use_full_page() and 
   * gtk_print_operation_set_unit() before starting the print operation
   * to set up the transformation of the cairo context according to your
   * needs.
   * 
   * Since: 2.10
   */
  signals[DRAW_PAGE] =
    g_signal_new (I_("draw-page"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, draw_page),
		  NULL, NULL,
		  _gtk_marshal_VOID__OBJECT_INT,
		  G_TYPE_NONE, 2,
		  GTK_TYPE_PRINT_CONTEXT,
		  G_TYPE_INT);

  /**
   * GtkPrintOperation::end-print:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @context: the #GtkPrintContext for the current operation
   *
   * Gets emitted after all pages have been rendered. 
   * A handler for this signal can clean up any resources that have
   * been allocated in the ::begin-print handler.
   * 
   * Since: 2.10
   */
  signals[END_PRINT] =
    g_signal_new (I_("end-print"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, end_print),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_PRINT_CONTEXT);

  /**
   * GtkPrintOperation::status-changed:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   *
   * Gets emitted at between the various phases of the print operation.
   * See #GtkPrintStatus for the phases that are being discriminated.
   * Use gtk_print_operation_get_status() to find out the current
   * status.
   *
   * Since: 2.10
   */
  signals[STATUS_CHANGED] =
    g_signal_new (I_("status-changed"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, status_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);


  /**
   * GtkPrintOperation::create-custom-widget:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   *
   * Gets emitted when displaying the print dialog. If you return a
   * widget in a handler for this signal it will be added to a custom
   * tab in the print dialog. You typically return a container widget
   * with multiple widgets in it.
   *
   * The print dialog owns the returned widget, and its lifetime
   * isn't controlled by the app. However, the widget is guaranteed
   * to stay around until the custom-widget-apply signal is emitted
   * on the operation. Then you can read out any information you need
   * from the widgets.
   *
   * Returns: A custom widget that gets embedded in the print dialog,
   *          or %NULL
   *
   * Since: 2.10
   */
  signals[CREATE_CUSTOM_WIDGET] =
    g_signal_new (I_("create-custom-widget"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, create_custom_widget),
		  custom_widget_accumulator, NULL,
		  _gtk_marshal_POINTER__VOID,
		  G_TYPE_POINTER, 0);

  /**
   * GtkPrintOperation::custom-widget-apply:
   * @operation: the #GtkPrintOperation on which the signal was emitted
   * @widget: the custom widget added in create-custom-widget
   *
   * This signal gets emitted right before begin-print if you added
   * a custom widget in the create-custom-widget handler. When you get
   * this signal you should read the information from the custom widgets,
   * as the widgets are not guaraneed to be around at a later time.
   *
   * Since: 2.10
   */
  signals[CUSTOM_WIDGET_APPLY] =
    g_signal_new (I_("custom-widget-apply"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkPrintOperationClass, custom_widget_apply),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

  /**
   * GtkPrintOperation:default-page-setup:
   *
   * The #GtkPageSetup used by default.
   * 
   * This page setup will be used by gtk_print_operation_run(),
   * but it can be overridden on a per-page basis by connecting
   * to the ::request-page-setup signal.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_DEFAULT_PAGE_SETUP,
				   g_param_spec_object ("default-page-setup",
							P_("Default Page Setup"),
							P_("The GtkPageSetup used by default"),
							GTK_TYPE_PAGE_SETUP,
							GTK_PARAM_READWRITE));

  /**
   * GtkPrintOperation:print-settings:
   *
   * The #GtkPrintSettings used for initializing the dialog.
   *
   * Setting this property is typically used to re-establish 
   * print settings from a previous print operation, see 
   * gtk_print_operation_run().
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_PRINT_SETTINGS,
				   g_param_spec_object ("print-settings",
							P_("Print Settings"),
							P_("The GtkPrintSettings used for initializing the dialog"),
							GTK_TYPE_PRINT_SETTINGS,
							GTK_PARAM_READWRITE));
  
  /**
   * GtkPrintOperation:job-name:
   *
   * A string used to identify the job (e.g. in monitoring 
   * applications like eggcups). 
   * 
   * If you don't set a job name, GTK+ picks a default one 
   * by numbering successive print jobs.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_JOB_NAME,
				   g_param_spec_string ("job-name",
							P_("Job Name"),
							P_("A string used for identifying the print job."),
							"",
							GTK_PARAM_READWRITE));

  /**
   * GtkPrintOperation:n-pages:
   *
   * The number of pages in the document. 
   *
   * This <emphasis>must</emphasis> be set to a positive number
   * before the rendering starts. It may be set in a ::begin-print 
   * signal hander.
   *
   * Note that the page numbers passed to the ::request-page-setup 
   * and ::draw-page signals are 0-based, i.e. if the user chooses 
   * to print all pages, the last ::draw-page signal will be for 
   * page @n_pages - 1.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_N_PAGES,
				   g_param_spec_int ("n-pages",
						     P_("Number of Pages"),
						     P_("The number of pages in the document."),
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE));

  /**
   * GtkPrintOperation:current-page:
   *
   * The current page in the document.
   *
   * If this is set before gtk_print_operation_run(), 
   * the user will be able to select to print only the current page.
   *
   * Note that this only makes sense for pre-paginated documents.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_CURRENT_PAGE,
				   g_param_spec_int ("current-page",
						     P_("Current Page"),
						     P_("The current page in the document."),
						     -1,
						     G_MAXINT,
						     -1,
						     GTK_PARAM_READWRITE));
  
  /**
   * GtkPrintOperation:use-full-page:
   *
   * If %TRUE, the transformation for the cairo context obtained 
   * from #GtkPrintContext puts the origin at the top left corner 
   * of the page (which may not be the top left corner of the sheet, 
   * depending on page orientation and the number of pages per sheet). 
   * Otherwise, the origin is at the top left corner of the imageable 
   * area (i.e. inside the margins).
   * 
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_USE_FULL_PAGE,
				   g_param_spec_boolean ("use-full-page",
							 P_("Use full page"),
							 P_("TRUE if the the origin of the context should be at the corner of the page and not the corner of the imageable area"),
							 FALSE,
							 GTK_PARAM_READWRITE));
  

  /**
   * GtkPrintOperation:track-print-status:
   *
   * If %TRUE, the print operation will try to continue report on 
   * the status of the print job in the printer queues and printer. 
   * This can allow your application to show things like "out of paper" 
   * issues, and when the print job actually reaches the printer. 
   * However, this is often implemented using polling, and should 
   * not be enabled unless needed.
   * 
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_USE_FULL_PAGE,
				   g_param_spec_boolean ("track-print-status",
							 P_("Track Print Status"),
							 P_("TRUE if the print operation will continue to report on the print job status after the print data has been sent to the printer or print server."),
							 FALSE,
							 GTK_PARAM_READWRITE));
  

  /**
   * GtkPrintOperation:unit:
   *
   * The transformation for the cairo context obtained from
   * #GtkPrintContext is set up in such a way that distances 
   * are measured in units of @unit.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_UNIT,
				   g_param_spec_enum ("unit",
						      P_("Unit"),
						      P_("The unit in which distances can be measured in the context"),
						      GTK_TYPE_UNIT,
						      GTK_UNIT_PIXEL,
						      GTK_PARAM_READWRITE));
  

  /**
   * GtkPrintOperation:show-dialog:
   *
   * Determines whether calling gtk_print_operation_run() will present
   * a print dialog to the user, or just print to the default printer.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_DIALOG,
				   g_param_spec_boolean ("show-dialog",
							 P_("Show Dialog"),
							 P_("TRUE if gtk_print_operation_run() should show the print dialog."),
							 TRUE,
							 GTK_PARAM_READWRITE));
  

  /**
   * GtkPrintOperation:show-progress:
   *
   * Determines whether to show a progress dialog during the 
   * print operation.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_SHOW_PROGRESS,
				   g_param_spec_boolean ("show-progress",
							 P_("Show Dialog"),
							 P_("TRUE if a progress dialog is shown while printing."),
							 FALSE,
							 GTK_PARAM_READWRITE));
  
  
  /**
   * GtkPrintOperation:pdf-target:
   *
   * The name of a PDF file to generate instead of showing 
   * the print dialog. 
   *
   * The intended use of this property is for implementing 
   * "Export to PDF" actions.
   *
   * "Print to PDF" support is independent of this and is done
   * by letting the user pick the "Print to PDF" item from the 
   * list of printers in the print dialog.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_JOB_NAME,
				   g_param_spec_string ("pdf-target",
							P_("PDF target filename"),
							P_("PDF target filename"),
							NULL,
							GTK_PARAM_READWRITE));
  
  /**
   * GtkPrintOperation:status:
   *
   * The status of the print operation.
   * 
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_STATUS,
				   g_param_spec_enum ("status",
						      P_("Status"),
						      P_("The status of the print operation"),
						      GTK_TYPE_PRINT_STATUS,
						      GTK_PRINT_STATUS_INITIAL,
						      GTK_PARAM_READABLE));
  
  /**
   * GtkPrintOperation:status-string:
   *
   * A string representation of the status of the print operation. 
   * The string is translated and suitable for displaying the print 
   * status e.g. in a #GtkStatusbar.
   *
   * See the ::status property for a status value that is suitable 
   * for programmatic use. 
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_STATUS_STRING,
				   g_param_spec_string ("status-string",
							P_("Status String"),
							P_("A human-readable description of the status"),
							"",
							GTK_PARAM_READABLE));
  

  /**
   * GtkPrintOperation:custom-tab-label:
   *
   * Used as the label of the tab containing custom widgets.
   * Note that this property may be ignored on some platforms.
   * 
   * If this is %NULL, GTK+ uses a default label.
   *
   * Since: 2.10
   */
  g_object_class_install_property (gobject_class,
				   PROP_CUSTOM_TAB_LABEL,
				   g_param_spec_string ("custom-tab-label",
							P_("Custom tab label"),
							P_("Label for the tab containing custom widgets."),
							NULL,
							GTK_PARAM_READWRITE));

}

/**
 * gtk_print_operation_new:
 *
 * Creates a new #GtkPrintOperation. 
 *
 * Returns: a new #GtkPrintOperation
 *
 * Since: 2.10
 */
GtkPrintOperation *
gtk_print_operation_new (void)
{
  GtkPrintOperation *print_operation;

  print_operation = g_object_new (GTK_TYPE_PRINT_OPERATION, NULL);
  
  return print_operation;
}

/**
 * gtk_print_operation_set_default_page_setup:
 * @op: a #GtkPrintOperation
 * @default_page_setup: a #GtkPageSetup, or %NULL
 * 
 * Makes @default_page_setup the default page setup for @op.
 *
 * This page setup will be used by gtk_print_operation_run(),
 * but it can be overridden on a per-page basis by connecting
 * to the ::request-page-setup signal.
 *
 * Since: 2.10
 **/
void
gtk_print_operation_set_default_page_setup (GtkPrintOperation *op,
					    GtkPageSetup      *default_page_setup)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (default_page_setup == NULL || 
                    GTK_IS_PAGE_SETUP (default_page_setup));

  priv = op->priv;

  if (default_page_setup != priv->default_page_setup)
    {
      if (default_page_setup)
	g_object_ref (default_page_setup);
      
      if (priv->default_page_setup)
	g_object_unref (priv->default_page_setup);
      
      priv->default_page_setup = default_page_setup;
     
      g_object_notify (G_OBJECT (op), "default-page-setup");
    }
}

/**
 * gtk_print_operation_get_default_page_setup:
 * @op: a #GtkPrintOperation
 *
 * Returns the default page setup, see 
 * gtk_print_operation_set_default_page_setup().
 *
 * Returns: the default page setup 
 *
 * Since: 2.10
 */
GtkPageSetup *
gtk_print_operation_get_default_page_setup (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), NULL);

  return op->priv->default_page_setup;
}


/**
 * gtk_print_operation_set_print_settings:
 * @op: a #GtkPrintOperation
 * @print_settings: #GtkPrintSettings, or %NULL
 * 
 * Sets the print settings for @op. This is typically used to
 * re-establish print settings from a previous print operation,
 * see gtk_print_operation_run().
 *
 * Since: 2.10
 **/
void
gtk_print_operation_set_print_settings (GtkPrintOperation *op,
					GtkPrintSettings  *print_settings)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (print_settings == NULL || 
                    GTK_IS_PRINT_SETTINGS (print_settings));

  priv = op->priv;

  if (print_settings != priv->print_settings)
    {
      if (print_settings)
        g_object_ref (print_settings);

      if (priv->print_settings)
        g_object_unref (priv->print_settings);
  
      priv->print_settings = print_settings;

      g_object_notify (G_OBJECT (op), "print-settings");
    }
}

/**
 * gtk_print_operation_get_print_settings:
 * @op: a #GtkPrintOperation
 * 
 * Returns the current print settings. 
 *
 * Note that the return value is %NULL until either 
 * gtk_print_operation_set_print_settings() or 
 * gtk_print_operation_run() have been called.
 * 
 * Return value: the current print settings of @op.
 * 
 * Since: 2.10
 **/
GtkPrintSettings *
gtk_print_operation_get_print_settings (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), NULL);

  return op->priv->print_settings;
}

/**
 * gtk_print_operation_set_job_name:
 * @op: a #GtkPrintOperation
 * @job_name: a string that identifies the print job
 * 
 * Sets the name of the print job. The name is used to identify 
 * the job (e.g. in monitoring applications like eggcups). 
 * 
 * If you don't set a job name, GTK+ picks a default one by 
 * numbering successive print jobs.
 *
 * Since: 2.10
 **/
void
gtk_print_operation_set_job_name (GtkPrintOperation *op,
				  const gchar       *job_name)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (g_utf8_validate (job_name, -1, NULL));

  priv = op->priv;

  g_free (priv->job_name);
  priv->job_name = g_strdup (job_name);

  g_object_notify (G_OBJECT (op), "job-name");
}

/**
 * gtk_print_operation_set_n_pages:
 * @op: a #GtkPrintOperation
 * @n_pages: the number of pages
 * 
 * Sets the number of pages in the document. 
 *
 * This <emphasis>must</emphasis> be set to a positive number
 * before the rendering starts. It may be set in a ::begin-print 
 * signal hander.
 *
 * Note that the page numbers passed to the ::request-page-setup 
 * and ::draw-page signals are 0-based, i.e. if the user chooses
 * to print all pages, the last ::draw-page signal will be
 * for page @n_pages - 1.
 *
 * Since: 2.10
 **/
void
gtk_print_operation_set_n_pages (GtkPrintOperation *op,
				 gint               n_pages)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (n_pages > 0);

  priv = op->priv;
  g_return_if_fail (priv->current_page == -1 || 
		    priv->current_page < n_pages);

  if (priv->nr_of_pages != n_pages)
    {
      priv->nr_of_pages = n_pages;

      g_object_notify (G_OBJECT (op), "n-pages");
    }
}

/**
 * gtk_print_operation_set_current_page:
 * @op: a #GtkPrintOperation
 * @current_page: the current page, 0-based
 *
 * Sets the current page.
 *
 * If this is called before gtk_print_operation_run(), 
 * the user will be able to select to print only the current page.
 *
 * Note that this only makes sense for pre-paginated documents.
 * 
 * Since: 2.10
 **/
void
gtk_print_operation_set_current_page (GtkPrintOperation *op,
				      gint               current_page)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  g_return_if_fail (current_page >= 0);

  priv = op->priv;
  g_return_if_fail (priv->nr_of_pages == -1 || 
		    current_page < priv->nr_of_pages);

  if (priv->current_page != current_page)
    {
      priv->current_page = current_page;

      g_object_notify (G_OBJECT (op), "current-page");
    }
}

/**
 * gtk_print_operation_set_use_full_page:
 * @op: a #GtkPrintOperation
 * @full_page: %TRUE to set up the #GtkPrintContext for the full page
 * 
 * If @full_page is %TRUE, the transformation for the cairo context 
 * obtained from #GtkPrintContext puts the origin at the top left 
 * corner of the page (which may not be the top left corner of the 
 * sheet, depending on page orientation and the number of pages per 
 * sheet). Otherwise, the origin is at the top left corner of the
 * imageable area (i.e. inside the margins).
 * 
 * Since: 2.10 
 */
void
gtk_print_operation_set_use_full_page (GtkPrintOperation *op,
				       gboolean           full_page)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  full_page = full_page != FALSE;
 
  priv = op->priv;
	
  if (priv->use_full_page != full_page)
    {
      priv->use_full_page = full_page;
   
      g_object_notify (G_OBJECT (op), "use-full-page");
    }
}

/**
 * gtk_print_operation_set_unit:
 * @op: a #GtkPrintOperation
 * @unit: the unit to use
 * 
 * Sets up the transformation for the cairo context obtained from
 * #GtkPrintContext in such a way that distances are measured in 
 * units of @unit.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_unit (GtkPrintOperation *op,
			      GtkUnit            unit)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  if (priv->unit != unit)
    {
      priv->unit = unit;

      g_object_notify (G_OBJECT (op), "unit");
    }
}

/**
 * gtk_print_operation_set_track_print_status:
 * @op: a #GtkPrintOperation
 * @track_status: %TRUE to track status after printing
 * 
 * If track_status is %TRUE, the print operation will try to continue report
 * on the status of the print job in the printer queues and printer. This
 * can allow your application to show things like "out of paper" issues,
 * and when the print job actually reaches the printer.
 * 
 * This function is often implemented using some form of polling, so it should
 * not be enabled unless needed.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_track_print_status (GtkPrintOperation  *op,
					    gboolean            track_status)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  if (priv->track_print_status != track_status)
    {
      priv->track_print_status = track_status;

      g_object_notify (G_OBJECT (op), "track-print-status");
    }
}

void
_gtk_print_operation_set_status (GtkPrintOperation *op,
				 GtkPrintStatus     status,
				 const gchar       *string)
{
  GtkPrintOperationPrivate *priv = op->priv;
  static const gchar *status_strs[] = {
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Initial state"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Preparing to print"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Generating data"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Sending data"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Waiting"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Blocking on issue"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Printing"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Finished"),
    /* translators, strip the prefix up to and including the first | */
    N_("print operation status|Finished with error")
  };

  if (status < 0 || status > GTK_PRINT_STATUS_FINISHED_ABORTED)
    status = GTK_PRINT_STATUS_FINISHED_ABORTED;

  if (string == NULL)
    string = g_strip_context (status_strs[status],
			      gettext (status_strs[status]));
  
  if (priv->status == status &&
      strcmp (string, priv->status_string) == 0)
    return;
  
  g_free (priv->status_string);
  priv->status_string = g_strdup (string);
  priv->status = status;

  g_object_notify (G_OBJECT (op), "status");
  g_object_notify (G_OBJECT (op), "status-string");

  g_signal_emit (op, signals[STATUS_CHANGED], 0);
}


/**
 * gtk_print_operation_get_status:
 * @op: a #GtkPrintOperation
 * 
 * Returns the status of the print operation. 
 * Also see gtk_print_operation_get_status_string().
 * 
 * Return value: the status of the print operation
 *
 * Since: 2.10
 **/
GtkPrintStatus
gtk_print_operation_get_status (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), 
                        GTK_PRINT_STATUS_FINISHED_ABORTED);

  return op->priv->status;
}

/**
 * gtk_print_operation_get_status_string:
 * @op: a #GtkPrintOperation
 * 
 * Returns a string representation of the status of the 
 * print operation. The string is translated and suitable
 * for displaying the print status e.g. in a #GtkStatusbar.
 *
 * Use gtk_print_operation_get_status() to obtain a status
 * value that is suitable for programmatic use. 
 * 
 * Return value: a string representation of the status
 *    of the print operation
 *
 * Since: 2.10
 **/
G_CONST_RETURN gchar *
gtk_print_operation_get_status_string (GtkPrintOperation *op)
{
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), "");

  return op->priv->status_string;
}

/**
 * gtk_print_operation_is_finished:
 * @op: a #GtkPrintOperation
 * 
 * A convenience function to find out if the print operation
 * is finished, either successfully (%GTK_PRINT_STATUS_FINISHED)
 * or unsuccessfully (%GTK_PRINT_STATUS_FINISHED_ABORTED).
 * 
 * Return value: %TRUE, if the print operation is finished.
 *
 * Since: 2.10
 **/
gboolean
gtk_print_operation_is_finished (GtkPrintOperation *op)
{
  GtkPrintOperationPrivate *priv;

  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), TRUE);

  priv = op->priv;
  return
    priv->status == GTK_PRINT_STATUS_FINISHED_ABORTED ||
    priv->status == GTK_PRINT_STATUS_FINISHED;
}


/**
 * gtk_print_operation_set_show_dialog:
 * @op: a #GtkPrintOperation
 * @show_dialog: %TRUE to show the print dialog
 * 
 * Sets whether calling gtk_print_operation_run() will present
 * a print dialog to the user, or just print to the default printer.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_show_dialog (GtkPrintOperation *op,
				     gboolean           show_dialog)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  show_dialog = show_dialog != FALSE;

  if (priv->show_dialog != show_dialog)
    {
      priv->show_dialog = show_dialog;

      g_object_notify (G_OBJECT (op), "show-dialog");
    }
}


/**
 * gtk_print_operation_set_show_progress:
 * @op: a #GtkPrintOperation
 * @show_progress: %TRUE to show a progress dialog
 * 
 * If @show_progress is %TRUE, the print operation will show a 
 * progress dialog during the print operation.
 * 
 * Since: 2.10
 */
void
gtk_print_operation_set_show_progress (GtkPrintOperation  *op,
				       gboolean            show_progress)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  show_progress = show_progress != FALSE;

  if (priv->show_progress != show_progress)
    {
      priv->show_progress = show_progress;

      g_object_notify (G_OBJECT (op), "show-progress");
    }
}

/**
 * gtk_print_operation_set_custom_tag_label:
 * @op: a #GtkPrintOperation
 * @label: the label to use, or %NULL to use the default label
 *
 * Sets the label for the tab holding custom widgets.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_custom_tab_label (GtkPrintOperation  *op,
					  const gchar        *label)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  g_free (priv->custom_tab_label);
  priv->custom_tab_label = g_strdup (label);

  g_object_notify (G_OBJECT (op), "custom-tab-label");
}


/**
 * gtk_print_operation_set_pdf_target:
 * @op: a #GtkPrintOperation
 * @filename: the filename for the PDF file
 * 
 * Sets up the #GtkPrintOperation to generate a PDF file instead
 * of showing the print dialog. The indended use of this function
 * is for implementing "Export to PDF" actions.
 *
 * "Print to PDF" support is independent of this and is done
 * by letting the user pick the "Print to PDF" item from the list
 * of printers in the print dialog.
 *
 * Since: 2.10
 */
void
gtk_print_operation_set_pdf_target (GtkPrintOperation *op,
				    const gchar       *filename)
{
  GtkPrintOperationPrivate *priv;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));

  priv = op->priv;

  g_free (priv->pdf_target);
  priv->pdf_target = g_strdup (filename);

  g_object_notify (G_OBJECT (op), "pdf-target");
}

/* Creates the initial page setup used for printing unless the
 * app overrides this on a per-page basis using request_page_setup.
 *
 * Data is taken from, in order, if existing:
 *
 * PrintSettings returned from the print dialog
 *  (initial dialog values are set from default_page_setup
 *   if unset in app specified print_settings)
 * default_page_setup
 * per-locale default setup
 */
static GtkPageSetup *
create_page_setup (GtkPrintOperation *op)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPageSetup *page_setup;
  GtkPrintSettings *settings;
  
  if (priv->default_page_setup)
    page_setup = gtk_page_setup_copy (priv->default_page_setup);
  else
    page_setup = gtk_page_setup_new ();

  settings = priv->print_settings;
  if (settings)
    {
      GtkPaperSize *paper_size;
      
      if (gtk_print_settings_has_key (settings, GTK_PRINT_SETTINGS_ORIENTATION))
	gtk_page_setup_set_orientation (page_setup,
					gtk_print_settings_get_orientation (settings));


      paper_size = gtk_print_settings_get_paper_size (settings);
      if (paper_size)
	{
	  gtk_page_setup_set_paper_size (page_setup, paper_size);
	  gtk_paper_size_free (paper_size);
	}

      /* TODO: Margins? */
    }
  
  return page_setup;
}

static void 
pdf_start_page (GtkPrintOperation *op,
		GtkPrintContext   *print_context,
		GtkPageSetup      *page_setup)
{
  GtkPaperSize *paper_size;
  double w, h;

  paper_size = gtk_page_setup_get_paper_size (page_setup);

  w = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
  h = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);
  
  cairo_pdf_surface_set_size (op->priv->surface, w, h);
}

static void
pdf_end_page (GtkPrintOperation *op,
	      GtkPrintContext   *print_context)
{
  cairo_t *cr;

  cr = gtk_print_context_get_cairo (print_context);
  cairo_show_page (cr);
}

static void
pdf_end_run (GtkPrintOperation *op,
	     gboolean           wait,
	     gboolean           cancelled)
{
  GtkPrintOperationPrivate *priv = op->priv;

  cairo_surface_destroy (priv->surface);
  priv->surface = NULL;
}

static GtkPrintOperationResult
run_pdf (GtkPrintOperation  *op,
	 GtkWindow          *parent,
	 gboolean           *do_print,
	 GError            **error)
{
  GtkPrintOperationPrivate *priv = op->priv;
  GtkPageSetup *page_setup;
  double width, height;
  /* This will be overwritten later by the non-default size, but
     we need to pass some size: */
  
  page_setup = create_page_setup (op);
  width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_POINTS);
  height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_POINTS);
  g_object_unref (page_setup);
  
  priv->surface = cairo_pdf_surface_create (priv->pdf_target,
					    width, height);
  cairo_pdf_surface_set_dpi (priv->surface, 300, 300);
  
  priv->dpi_x = 72;
  priv->dpi_y = 72;

  priv->print_pages = GTK_PRINT_PAGES_ALL;
  priv->page_ranges = NULL;
  priv->num_page_ranges = 0;

  priv->manual_num_copies = 1;
  priv->manual_collation = FALSE;
  priv->manual_reverse = FALSE;
  priv->manual_page_set = GTK_PAGE_SET_ALL;
  priv->manual_scale = 1.0;
  priv->manual_orientation = TRUE;
  
  *do_print = TRUE;
  
  priv->start_page = pdf_start_page;
  priv->end_page = pdf_end_page;
  priv->end_run = pdf_end_run;
  
  return GTK_PRINT_OPERATION_RESULT_APPLY; 
}

typedef struct
{
  GtkPrintOperation *op;
  gboolean wait;
  gint uncollated_copies;
  gint collated_copies;
  gint uncollated, collated, total;

  gint range, num_ranges;
  GtkPageRange *ranges;
  GtkPageRange one_range;

  gint page, start, end, inc;

  GtkPageSetup *initial_page_setup;
  GtkPrintContext *print_context;  

  GtkWidget *progress;
} PrintPagesData;

static void
find_range (PrintPagesData *data)
{
  GtkPageRange *range;

  range = &data->ranges[data->range];

  if (data->inc < 0)
    {
      data->start = range->end;
      data->end = range->start - 1;
    }
  else
    {
      data->start = range->start;
      data->end = range->end + 1;
    }
}

static gboolean 
increment_page_sequence (PrintPagesData *data)
{
  GtkPrintOperationPrivate *priv = data->op->priv;

  do {
    data->page += data->inc;
    if (data->page == data->end)
      {
	data->range += data->inc;
	if (data->range == -1 || data->range == data->num_ranges)
	  {
	    data->uncollated++;
	    if (data->uncollated == data->uncollated_copies)
	      return FALSE;

	    data->range = data->inc < 0 ? data->num_ranges - 1 : 0;
	  }
	find_range (data);
	data->page = data->start;
      }
  }
  while ((priv->manual_page_set == GTK_PAGE_SET_EVEN && data->page % 2 == 0) ||
	 (priv->manual_page_set == GTK_PAGE_SET_ODD && data->page % 2 == 1));

  return TRUE;
}

static void
print_pages_idle_done (gpointer user_data)
{
  PrintPagesData *data;
  GtkPrintOperationPrivate *priv;

  GDK_THREADS_ENTER ();

  data = (PrintPagesData*)user_data;
  priv = data->op->priv;

  priv->print_pages_idle_id = 0;

  if (priv->show_progress_timeout_id > 0)
    {
      g_source_remove (priv->show_progress_timeout_id);
      priv->show_progress_timeout_id = 0;
    }

  if (data->progress)
    gtk_widget_destroy (data->progress);

  g_object_unref (data->print_context);
  g_object_unref (data->initial_page_setup);

  g_object_unref (data->op);
  g_free (data);

  GDK_THREADS_LEAVE ();
}

static void
update_progress (PrintPagesData *data)
{
  GtkPrintOperationPrivate *priv; 
  gchar *text = NULL;
  
  priv = data->op->priv;
 
  if (data->progress)
    {
      if (priv->status == GTK_PRINT_STATUS_PREPARING)
	{
	  if (priv->nr_of_pages > 0)
	    text = g_strdup_printf (_("Preparing %d"), priv->nr_of_pages);
	  else
	    text = g_strdup (_("Preparing"));
	}
      else if (priv->status == GTK_PRINT_STATUS_GENERATING_DATA)
	text = g_strdup_printf (_("Printing %d"), data->total);
      
      if (text)
	{
	  g_object_set (data->progress, "text", text, NULL);
	  g_free (text);
	}
    }
 }

static gboolean
print_pages_idle (gpointer user_data)
{
  PrintPagesData *data; 
  GtkPrintOperationPrivate *priv; 
  GtkPageSetup *page_setup;
  cairo_t *cr;
  gboolean done = FALSE;

  GDK_THREADS_ENTER ();

  data = (PrintPagesData*)user_data;
  priv = data->op->priv;

  if (priv->status == GTK_PRINT_STATUS_PREPARING)
    {
      if (!data->print_context)
	{
	  data->print_context = _gtk_print_context_new (data->op);
	  data->initial_page_setup = create_page_setup (data->op);

	  _gtk_print_context_set_page_setup (data->print_context, 
					     data->initial_page_setup);
      
	  g_signal_emit (data->op, signals[BEGIN_PRINT], 0, data->print_context);
      
	  if (priv->manual_collation)
	    {
	      data->uncollated_copies = priv->manual_num_copies;
	      data->collated_copies = 1;
	    }
	  else
	    {
	      data->uncollated_copies = 1;
	      data->collated_copies = priv->manual_num_copies;
	    }

	  goto out;
	}
      
      if (g_signal_has_handler_pending (data->op, signals[PAGINATE], 0, FALSE))
	{
	  gboolean paginated = FALSE;

	  g_signal_emit (data->op, signals[PAGINATE], 0, data->print_context, &paginated);
	  if (!paginated)
	    goto out;
	}

      /* FIXME handle this better */
      if (priv->nr_of_pages == 0)
	g_warning ("no pages to print");
      
      /* Initialize parts of PrintPagesData that depend on nr_of_pages
       */
      if (priv->print_pages == GTK_PRINT_PAGES_RANGES)
	{
	  data->ranges = priv->page_ranges;
	  data->num_ranges = priv->num_page_ranges;
	}
      else if (priv->print_pages == GTK_PRINT_PAGES_CURRENT &&
	       priv->current_page != -1)
	{
	  data->ranges = &data->one_range;
	  data->num_ranges = 1;
	  data->ranges[0].start = priv->current_page;
	  data->ranges[0].end = priv->current_page;
	}
      else
	{
	  data->ranges = &data->one_range;
	  data->num_ranges = 1;
	  data->ranges[0].start = 0;
	  data->ranges[0].end = priv->nr_of_pages - 1;
	}
      
      if (data->op->priv->manual_reverse)
	{
	  data->range = data->num_ranges - 1;
	  data->inc = -1;      
	}
      else
	{
	  data->range = 0;
	  data->inc = 1;      
	}
      find_range (data);
     
      /* go back one page, since we preincrement below */
      data->page = data->start - data->inc;
      data->collated = data->collated_copies - 1;

      _gtk_print_operation_set_status (data->op, 
				       GTK_PRINT_STATUS_GENERATING_DATA, 
				       NULL);

      goto out;
    }

  data->total++;
  data->collated++;
  if (data->collated == data->collated_copies)
    {
      data->collated = 0;
      if (!increment_page_sequence (data))
	{
	  done = TRUE;

	  goto out;
	}
    }

  page_setup = gtk_page_setup_copy (data->initial_page_setup);
  g_signal_emit (data->op, signals[REQUEST_PAGE_SETUP], 0, 
		 data->print_context, data->page, page_setup);
  
  _gtk_print_context_set_page_setup (data->print_context, page_setup);
  data->op->priv->start_page (data->op, data->print_context, page_setup);
  
  cr = gtk_print_context_get_cairo (data->print_context);
  
  cairo_save (cr);
  if (priv->manual_scale != 100.0)
    cairo_scale (cr,
		 priv->manual_scale,
		 priv->manual_scale);
  
  if (priv->manual_orientation)
    _gtk_print_context_rotate_according_to_orientation (data->print_context);
  
  if (!priv->use_full_page)
    _gtk_print_context_translate_into_margin (data->print_context);
  
  g_signal_emit (data->op, signals[DRAW_PAGE], 0, 
		 data->print_context, data->page);
  
  priv->end_page (data->op, data->print_context);
  
  cairo_restore (cr);
  
  g_object_unref (page_setup);

 out:

  if (priv->cancelled)
    {
      _gtk_print_operation_set_status (data->op, GTK_PRINT_STATUS_FINISHED_ABORTED, NULL);
      
      done = TRUE;
    }

  if (done)
    {
      g_signal_emit (data->op, signals[END_PRINT], 0, data->print_context);
      
      cairo_surface_finish (data->op->priv->surface);
      priv->end_run (data->op, data->wait, priv->cancelled);
    }

  update_progress (data);

  GDK_THREADS_LEAVE ();

  return !done;
}
  
static void
handle_progress_response (GtkWidget *dialog, 
			  gint       response,
			  gpointer   data)
{
  GtkPrintOperation *op = (GtkPrintOperation *)data;

  gtk_widget_hide (dialog);
  gtk_print_operation_cancel (op);
}

static gboolean
show_progress_timeout (PrintPagesData *data)
{
  GDK_THREADS_ENTER ();

  gtk_window_present (GTK_WINDOW (data->progress));

  data->op->priv->show_progress_timeout_id = 0;

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
print_pages (GtkPrintOperation *op,
	     GtkWindow         *parent,
	     gboolean           wait)
{
  GtkPrintOperationPrivate *priv = op->priv;
  PrintPagesData *data;
 
  _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_PREPARING, NULL);  

  data = g_new0 (PrintPagesData, 1);
  data->op = g_object_ref (op);
  data->wait = wait;

  if (priv->show_progress)
    {
      GtkWidget *progress;

      progress = gtk_message_dialog_new (parent, 0, 
					 GTK_MESSAGE_OTHER,
					 GTK_BUTTONS_CANCEL,
					 _("Preparing"));
      g_signal_connect (progress, "response", 
			G_CALLBACK (handle_progress_response), op);

      priv->show_progress_timeout_id = 
	g_timeout_add (SHOW_PROGRESS_TIME, 
		       (GSourceFunc)show_progress_timeout,
		       data);

      data->progress = progress;
    }

  if (wait)
    {
      /* FIXME replace this with a recursive mainloop */
      while (print_pages_idle (data))
	{
	  /* Iterate the mainloop so that we redraw windows */
	  while (gtk_events_pending ())
	    gtk_main_iteration ();
	}
      print_pages_idle_done (data);
    }
  else
    priv->print_pages_idle_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
						 print_pages_idle, 
						 data, 
						 print_pages_idle_done);}

/**
 * gtk_print_operation_run:
 * @op: a #GtkPrintOperation
 * @parent: Transient parent of the dialog, or %NULL
 * @error: Return location for errors, or %NULL
 * 
 * Runs the print operation, by first letting the user modify
 * print settings in the print dialog, and then print the
 * document.
 *
 * Note that this function does not return until the rendering of all 
 * pages is complete. You can connect to the ::status-changed signal on
 * @op to obtain some information about the progress of the print operation. 
 * Furthermore, it may use a recursive mainloop to show the print dialog.
 * See gtk_print_operation_run_async() if this is a problem.
 *
 * <informalexample><programlisting>
 *  FIXME: need an example here
 * </programlisting></informalexample>
 *
 * Return value: the result of the print operation. A return value of 
 *   %GTK_PRINT_OPERATION_RESULT_APPLY indicates that the printing was
 *   completed successfully. In this case, it is a good idea to obtain 
 *   the used print settings with gtk_print_operation_get_print_settings() 
 *   and store them for reuse with the next print operation.
 *
 * Since: 2.10
 **/
GtkPrintOperationResult
gtk_print_operation_run (GtkPrintOperation  *op,
			 GtkWindow          *parent,
			 GError            **error)
{
  GtkPrintOperationPrivate *priv;
  GtkPrintOperationResult result;
  gboolean do_print;
  
  g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), 
                        GTK_PRINT_OPERATION_RESULT_ERROR);

  priv = op->priv;

  if (priv->pdf_target != NULL)
    result = run_pdf (op, parent, &do_print, error);
  else
    result = _gtk_print_operation_platform_backend_run_dialog (op, 
							       parent,
							       &do_print,
							       error);
  if (do_print)
    print_pages (op, parent, TRUE);
  else 
    _gtk_print_operation_set_status (op, GTK_PRINT_STATUS_FINISHED_ABORTED, NULL);

  return result;
}

/**
 * gtk_print_operation_run_async:
 * @op: a #GtkPrintOperation
 * @parent: Transient parent of the dialog, or %NULL
 * 
 * Runs the print operation, by first letting the user modify
 * print settings in the print dialog, and then print the
 * document.
 *
 * In contrast to gtk_print_operation_run(), this function returns after 
 * showing the print dialog on platforms that support this, and handles 
 * the printing by connecting a signal handler to the ::response signal 
 * of the dialog. 
 * 
 * If you use this function, it is recommended that you store the modified
 * #GtkPrintSettings in a ::begin-print or ::end-print signal handler.
 * 
 * Since: 2.10
 **/
void
gtk_print_operation_run_async (GtkPrintOperation *op,
			       GtkWindow         *parent)
{
  GtkPrintOperationPrivate *priv;
  gboolean do_print;

  g_return_if_fail (GTK_IS_PRINT_OPERATION (op)); 

  priv = op->priv;

  if (priv->pdf_target != NULL)
    {
      run_pdf (op, parent, &do_print, NULL);
      if (do_print)
	print_pages (op, parent, FALSE);
      else 
	_gtk_print_operation_set_status (op, GTK_PRINT_STATUS_FINISHED_ABORTED, NULL);
    }
  else
    _gtk_print_operation_platform_backend_run_dialog_async (op, 
							    parent,
							    print_pages);
}


/**
 * gtk_print_operation_cancel:
 * @op: a #GtkPrintOperation
 *
 * Cancels a running print operation. This function may
 * be called from a begin-print, paginate or draw-page
 * signal handler to stop the currently running print 
 * operation.
 *
 * Since: 2.10
 */
void
gtk_print_operation_cancel (GtkPrintOperation *op)
{
  g_return_if_fail (GTK_IS_PRINT_OPERATION (op));
  
  op->priv->cancelled = TRUE;
}



#define __GTK_PRINT_OPERATION_C__
#include "gtkaliasdef.c"
