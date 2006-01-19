/* 
 * GTK - The GIMP Toolkit
 * Copyright (C) 1999  Red Hat, Inc.
 * Copyright (C) 2002  Anders Carlsson <andersca@gnu.org>
 * Copyright (C) 2003  Matthias Clasen <mclasen@redhat.com>
 * Copyright (C) 2005  Carlos Garnacho Parro <carlosg@gnome.org>
 *
 * All rights reserved.
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

#include <config.h>

#include "gtkassistant.h"

#include "gtkbutton.h"
#include "gtkhbox.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtksizegroup.h"
#include "gtkstock.h"

#include "gtkintl.h"
#include "gtkprivate.h"

#include "gtkalias.h"

#define GTK_ASSISTANT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GTK_TYPE_ASSISTANT, GtkAssistantPrivate))

#define HEADER_SPACING 12

typedef struct _GtkAssistantPage GtkAssistantPage;

struct _GtkAssistantPage
{
  GtkWidget *page;
  GtkAssistantPageType type;
  gboolean   complete;

  GtkWidget *title;
  GdkPixbuf *header_image;
  GdkPixbuf *sidebar_image;
};

struct _GtkAssistantPrivate
{
  GtkWidget *header_image;
  GtkWidget *sidebar_image;

  GtkWidget *action_area;

  GList     *pages;

  GtkAssistantPage *current_page;

  GSList    *visited_pages;

  GtkSizeGroup *size_group;

  GtkAssistantPageFunc forward_function;
  gpointer forward_function_data;
  GDestroyNotify forward_data_destroy;
};

static void     gtk_assistant_class_init         (GtkAssistantClass *class);
static void     gtk_assistant_init               (GtkAssistant      *assistant);
static void     gtk_assistant_destroy            (GtkObject         *object);
static void     gtk_assistant_style_set          (GtkWidget         *widget,
						  GtkStyle          *old_style);
static void     gtk_assistant_size_request       (GtkWidget         *widget,
						  GtkRequisition    *requisition);
static void     gtk_assistant_size_allocate      (GtkWidget         *widget,
						  GtkAllocation     *allocation);
static void     gtk_assistant_map                (GtkWidget         *widget);
static void     gtk_assistant_unmap              (GtkWidget         *widget);
static gboolean gtk_assistant_delete_event       (GtkWidget         *widget,
						  GdkEventAny       *event);
static gboolean gtk_assistant_expose             (GtkWidget         *widget,
						  GdkEventExpose    *event);
static void     gtk_assistant_add                (GtkContainer      *container,
						  GtkWidget         *page);
static void     gtk_assistant_remove             (GtkContainer      *container,
						  GtkWidget         *page);
static void     gtk_assistant_forall             (GtkContainer      *container,
						  gboolean           include_internals,
						  GtkCallback        callback,
						  gpointer           callback_data);
static void     gtk_assistant_set_child_property (GtkContainer      *container,
						  GtkWidget         *child,
						  guint              property_id,
						  const GValue      *value,
						  GParamSpec        *pspec);
static void     gtk_assistant_get_child_property (GtkContainer      *container,
						  GtkWidget         *child,
						  guint              property_id,
						  GValue            *value,
						  GParamSpec        *pspec);

enum
{
  CHILD_PROP_0,
  CHILD_PROP_PAGE_TYPE,
  CHILD_PROP_PAGE_TITLE,
  CHILD_PROP_PAGE_HEADER_IMAGE,
  CHILD_PROP_PAGE_SIDEBAR_IMAGE,
  CHILD_PROP_PAGE_COMPLETE
};

enum
{
  CANCEL,
  PREPARE,
  APPLY,
  CLOSE,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE (GtkAssistant, gtk_assistant, GTK_TYPE_WINDOW);


static void
gtk_assistant_class_init (GtkAssistantClass *class)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  gobject_class   = (GObjectClass *) class;
  object_class    = (GtkObjectClass *) class;
  widget_class    = (GtkWidgetClass *) class;
  container_class = (GtkContainerClass *) class;

  object_class->destroy = gtk_assistant_destroy;

  widget_class->style_set = gtk_assistant_style_set;
  widget_class->size_request = gtk_assistant_size_request;
  widget_class->size_allocate = gtk_assistant_size_allocate;
  widget_class->map = gtk_assistant_map;
  widget_class->unmap = gtk_assistant_unmap;
  widget_class->delete_event = gtk_assistant_delete_event;
  widget_class->expose_event = gtk_assistant_expose;

  container_class->add = gtk_assistant_add;
  container_class->remove = gtk_assistant_remove;
  container_class->forall = gtk_assistant_forall;
  container_class->set_child_property = gtk_assistant_set_child_property;
  container_class->get_child_property = gtk_assistant_get_child_property;

  /**
   * GtkAssistant::cancel:
   * @assistant: the #GtkAssistant
   * @page: the current page
   *
   * The ::cancel signal is emitted when then the cancel button is clicked.
   *
   * Since: 2.10
   */
  signals[CANCEL] =
    g_signal_new (I_("cancel"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkAssistantClass, cancel),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
    
  /**
   * GtkAssistant::prepare:
   * @assistant: the #GtkAssistant
   * @page: the current page
   *
   * The ::prepared signal is emitted when a new page is set as the assistant's 
   * current page, before making the new page visible. A handler for this signal 
   * can do any preparation which are necessary before showing @page.
   *
   * Since: 2.10
   */
  signals[PREPARE] =
    g_signal_new (I_("prepare"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkAssistantClass, prepare),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_WIDGET);

  /**
   * GtkAssistant::apply:
   * @assistant: the @GtkAssistant
   * @page: the current page
   *
   * The ::apply signal is emitted when the apply button is clicked. The default
   * behavior of the #GtkAssistant is to switch to the page after the current page,
   * unless the current page is the last one.
   *
   * A handler for the ::apply signal should carry out the actions for which the
   * wizard has collected data. If the action takes a long time to complete, you
   * might consider to put a page displaying the progress of the operation after the
   * confirmation page with the apply button.
   *
   * Return value: %TRUE to suppress the default behavior
   *
   * Since: 2.10
   */
  signals[APPLY] =
    g_signal_new (I_("apply"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkAssistantClass, apply),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /**
   * GtkAssistant::close:
   * @assistant: the #GtkAssistant
   * @page: the current page
   *
   * The ::close signal is emitted when the close button is clicked.
   *
   * Since: 2.10
   */
  signals[CLOSE] =
    g_signal_new (I_("close"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GtkAssistantClass, close),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("header-padding",
							     P_("Header Padding"),
							     P_("Number of pixels around the header."),
							     0,
							     G_MAXINT,
							     6,
							     GTK_PARAM_READABLE));
  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("content-padding",
							     P_("Content Padding"),
							     P_("Number of pixels around the content pages."),
							     0,
							     G_MAXINT,
							     1,
							     GTK_PARAM_READABLE));

  /**
   * GtkAssistant:page-type:
   *
   * The type of the assistant page. 
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_TYPE,
					      g_param_spec_enum ("page-type", 
								 P_("Page type"),
								 P_("The type of the assistant page"),
								 GTK_TYPE_ASSISTANT_PAGE_TYPE,
								 GTK_ASSISTANT_PAGE_CONTENT,
								 GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:title:
   *
   * The title that is displayed in the page header. 
   *
   * If title and header-image are both %NULL, no header is displayed.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_TITLE,
					      g_param_spec_string ("title", 
								   P_("Page title"),
								   P_("The title of the assistant page"),
								   NULL,
								   GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:header-image:
   *
   * The image that is displayed next to the title in the page header.
   *
   * If title and header-image are both %NULL, no header is displayed.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_HEADER_IMAGE,
					      g_param_spec_object ("header-image", 
								   P_("Header image"),
								   P_("Header image for the assistant page"),
								   GDK_TYPE_PIXBUF,
								   GTK_PARAM_READWRITE));

  /**
   * GtkAssistant:header-image:
   *
   * The image that is displayed next to the page. 
   *
   * Set this to %NULL to make the sidebar disappear.
   *
   * Since: 2.10
   */
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_SIDEBAR_IMAGE,
					      g_param_spec_object ("sidebar-image", 
								   P_("Sidebar image"),
								   P_("Sidebar image for the assistant page"),
								   GDK_TYPE_PIXBUF,
								   GTK_PARAM_READWRITE));
  /**
   * GtkAssistant:complete:
   *
   * Setting the "complete" child property to %TRUE marks a page as complete
   * (i.e.: all the required fields are filled out). GTK+ uses this information
   * to control the sensitivity of the navigation buttons.
   *
   * Since: 2.10
   **/
  gtk_container_class_install_child_property (container_class,
					      CHILD_PROP_PAGE_COMPLETE,
					      g_param_spec_boolean ("complete", 
								    P_("Page complete"),
								    P_("Whether all required fields on the page have been filled out"),
								    FALSE,
								    G_PARAM_READWRITE));

  g_type_class_add_private (gobject_class, sizeof (GtkAssistantPrivate));
}

static gint
default_forward_function (gint current_page, gpointer data)
{
  GtkAssistant *assistant;
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *page_node;

  assistant = GTK_ASSISTANT (data);
  priv = assistant->priv;

  page_node = g_list_nth (priv->pages, ++current_page);

  if (!page_node)
    return -1;

  page_info = (GtkAssistantPage *) page_node->data;

  while (!GTK_WIDGET_VISIBLE (page_info->page))
    {
      page_node = page_node->next;
      page_info = (GtkAssistantPage *) page_node->data;
      current_page++;
    }

  return current_page;
}

static void
compute_last_button_state (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;
  gint count, page_num, n_pages;

  count = 0;
  page_num = gtk_assistant_get_current_page (assistant);
  n_pages  = gtk_assistant_get_n_pages (assistant);
  page_info = g_list_nth_data (priv->pages, page_num);

  while ((page_num > 0 && page_num < n_pages) &&
	 (page_info->type == GTK_ASSISTANT_PAGE_CONTENT) &&
	 (page_info->complete))
    {
      page_num  = (priv->forward_function) (page_num, priv->forward_function_data);
      page_info = g_list_nth_data (priv->pages, page_num);
      count++;
    }

  if (count > 1)
    gtk_widget_show (assistant->last);
  else
    gtk_widget_hide (assistant->last);
}

static void
_set_assistant_header_image (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->header_image),
			     priv->current_page->header_image);
}

static void
_set_assistant_sidebar_image (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  gtk_image_set_from_pixbuf (GTK_IMAGE (priv->sidebar_image),
			     priv->current_page->sidebar_image);

  if (priv->current_page->sidebar_image)
    gtk_widget_show (priv->sidebar_image);
  else
    gtk_widget_hide (priv->sidebar_image);
}

static void
_set_assistant_buttons_state (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  switch (priv->current_page->type)
    {
    case GTK_ASSISTANT_PAGE_INTRO:
      gtk_widget_set_sensitive (assistant->cancel, TRUE);
      gtk_widget_set_sensitive (assistant->forward, priv->current_page->complete);
      gtk_widget_show (assistant->cancel);
      gtk_widget_show (assistant->forward);
      gtk_widget_hide (assistant->back);
      gtk_widget_hide (assistant->apply);
      gtk_widget_hide (assistant->close);
      compute_last_button_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_CONFIRM:
      gtk_widget_set_sensitive (assistant->cancel, TRUE);
      gtk_widget_set_sensitive (assistant->back, TRUE);
      gtk_widget_set_sensitive (assistant->apply, priv->current_page->complete);
      gtk_widget_show (assistant->cancel);
      gtk_widget_show (assistant->back);
      gtk_widget_show (assistant->apply);
      gtk_widget_hide (assistant->forward);
      gtk_widget_hide (assistant->close);
      gtk_widget_hide (assistant->last);
      break;
    case GTK_ASSISTANT_PAGE_CONTENT:
      gtk_widget_set_sensitive (assistant->cancel, TRUE);
      gtk_widget_set_sensitive (assistant->back, TRUE);
      gtk_widget_set_sensitive (assistant->forward, priv->current_page->complete);
      gtk_widget_show (assistant->cancel);
      gtk_widget_show (assistant->back);
      gtk_widget_show (assistant->forward);
      gtk_widget_hide (assistant->apply);
      gtk_widget_hide (assistant->close);
      compute_last_button_state (assistant);
      break;
    case GTK_ASSISTANT_PAGE_SUMMARY:
      gtk_widget_set_sensitive (assistant->close, TRUE);
      gtk_widget_show (assistant->close);
      gtk_widget_hide (assistant->cancel);
      gtk_widget_hide (assistant->back);
      gtk_widget_hide (assistant->forward);
      gtk_widget_hide (assistant->apply);
      gtk_widget_hide (assistant->last);
      break;
    case GTK_ASSISTANT_PAGE_PROGRESS:
      gtk_widget_set_sensitive (assistant->cancel, priv->current_page->complete);
      gtk_widget_set_sensitive (assistant->back, priv->current_page->complete);
      gtk_widget_set_sensitive (assistant->forward, priv->current_page->complete);
      gtk_widget_show (assistant->cancel);
      gtk_widget_show (assistant->back);
      gtk_widget_show (assistant->forward);
      gtk_widget_hide (assistant->apply);
      gtk_widget_hide (assistant->close);
      gtk_widget_hide (assistant->last);
      break;
    default:
      g_assert_not_reached ();
    }

  /* this is quite general, we don't want to
   * go back if it's the first page */
  if (!priv->visited_pages)
    gtk_widget_hide (assistant->back);
}

static void
_set_current_page (GtkAssistant     *assistant,
		   GtkAssistantPage *page)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *old_page;

  if (priv->current_page &&
      GTK_WIDGET_DRAWABLE (priv->current_page->page))
    old_page = priv->current_page;
  else
    old_page = NULL;

  priv->current_page = page;

  _set_assistant_buttons_state (assistant);
  _set_assistant_header_image (assistant);
  _set_assistant_sidebar_image (assistant);

  g_signal_emit (assistant, signals [PREPARE], 0, priv->current_page->page);

  if (GTK_WIDGET_VISIBLE (priv->current_page->page) && GTK_WIDGET_MAPPED (assistant))
    {
      gtk_widget_map (priv->current_page->page);
      gtk_widget_map (priv->current_page->title);
    }
  
  if (old_page && GTK_WIDGET_MAPPED (old_page->page))
    {
      gtk_widget_unmap (old_page->page);
      gtk_widget_unmap (old_page->title);
    }

  gtk_widget_queue_resize (GTK_WIDGET (assistant));
}

static gint
compute_next_step (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;
  gint current_page, n_pages, next_page;

  current_page = gtk_assistant_get_current_page (assistant);
  page_info = priv->current_page;
  n_pages = gtk_assistant_get_n_pages (assistant);

  next_page = (priv->forward_function) (current_page,
					priv->forward_function_data);

  if (next_page >= 0 && next_page < n_pages)
    {
      priv->visited_pages = g_slist_prepend (priv->visited_pages, page_info);
      _set_current_page (assistant, g_list_nth_data (priv->pages, next_page));

      return TRUE;
    }

  return FALSE;
}

static void
on_assistant_close (GtkWidget *widget, GtkAssistant *assistant)
{
  g_signal_emit (assistant, signals [CLOSE], 0, NULL);
}

static void
on_assistant_apply (GtkWidget *widget, GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  gboolean success;

  success = compute_next_step (assistant);

  g_signal_emit (assistant, signals [APPLY], 0, priv->current_page->page);

  /* if the assistant hasn't switched to another page, just emit
   * the CLOSE signal, it't the last page in the assistant flow
   */
  if (!success)
    g_signal_emit (assistant, signals [CLOSE], 0, priv->current_page->page);
}

static void
on_assistant_forward (GtkWidget *widget, GtkAssistant *assistant)
{
  if (!compute_next_step (assistant))
    g_critical ("Page flow is broken, you may want to end it with a page of "
		"type GTK_ASSISTANT_PAGE_CONFIRM or GTK_ASSISTANT_PAGE_SUMMARY");
}

static void
on_assistant_back (GtkWidget *widget, GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;
  GSList *page_node;

  /* skip the progress pages when going back */
  do
    {
      page_node = priv->visited_pages;

      g_return_if_fail (page_node != NULL);

      priv->visited_pages = priv->visited_pages->next;
      page_info = (GtkAssistantPage *) page_node->data;
      g_slist_free_1 (page_node);
    }
  while (page_info->type == GTK_ASSISTANT_PAGE_PROGRESS ||
	 !GTK_WIDGET_VISIBLE (page_info->page));

  _set_current_page (assistant, page_info);
}

static void
on_assistant_cancel (GtkWidget *widget, GtkAssistant *assistant)
{
  g_signal_emit (assistant, signals [CANCEL], 0, NULL);
}

static void
on_assistant_last (GtkWidget *widget, GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv = assistant->priv;

  while (priv->current_page->type == GTK_ASSISTANT_PAGE_CONTENT &&
	 priv->current_page->complete)
    compute_next_step (assistant);
}

static gboolean
alternative_button_order (GtkAssistant *assistant)
{
  GtkSettings *settings;
  GdkScreen *screen;
  gboolean result;

  screen   = gtk_widget_get_screen (GTK_WIDGET (assistant));
  settings = gtk_settings_get_for_screen (screen);

  g_object_get (settings,
		"gtk-alternative-button-order", &result,
		NULL);
  return result;
}

static void
gtk_assistant_init (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;

  priv = assistant->priv = GTK_ASSISTANT_GET_PRIVATE (assistant);

  gtk_widget_push_composite_child ();

  /* Header */
  priv->header_image = gtk_image_new ();
  gtk_misc_set_alignment (GTK_MISC (priv->header_image), 1., 0.5);
  gtk_widget_set_parent (priv->header_image, GTK_WIDGET (assistant));
  gtk_widget_show (priv->header_image);

  /* Sidebar */
  priv->sidebar_image = gtk_image_new ();
  gtk_misc_set_alignment (GTK_MISC (priv->sidebar_image), 0., 0.);
  gtk_widget_set_parent (priv->sidebar_image, GTK_WIDGET (assistant));
  gtk_widget_show (priv->sidebar_image);

  /* Action area */
  priv->action_area  = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (priv->action_area), 6);
  assistant->close   = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  assistant->apply   = gtk_button_new_from_stock (GTK_STOCK_APPLY);
  assistant->forward = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
  assistant->back    = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
  assistant->cancel  = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  assistant->last    = gtk_button_new_from_stock (GTK_STOCK_GOTO_LAST);

  priv->size_group   = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (priv->size_group, assistant->close);
  gtk_size_group_add_widget (priv->size_group, assistant->apply);
  gtk_size_group_add_widget (priv->size_group, assistant->forward);
  gtk_size_group_add_widget (priv->size_group, assistant->back);
  gtk_size_group_add_widget (priv->size_group, assistant->cancel);
  gtk_size_group_add_widget (priv->size_group, assistant->last);

  if (!alternative_button_order (assistant))
    {
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->close, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->apply, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->forward, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->back, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->cancel, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->last, FALSE, FALSE, 0);
    }
  else
    {
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->cancel, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->close, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->apply, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->forward, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->back, FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (priv->action_area), assistant->last, FALSE, FALSE, 0);
    }

  gtk_widget_set_parent (priv->action_area, GTK_WIDGET (assistant));
  gtk_widget_show (assistant->forward);
  gtk_widget_show (assistant->back);
  gtk_widget_show (assistant->cancel);
  gtk_widget_show (priv->action_area);

  gtk_widget_pop_composite_child ();

  priv->pages = NULL;
  priv->current_page = NULL;
  priv->visited_pages = NULL;

  priv->forward_function = default_forward_function;
  priv->forward_function_data = assistant;
  priv->forward_data_destroy = NULL;

  g_signal_connect (G_OBJECT (assistant->close), "clicked",
		    G_CALLBACK (on_assistant_close), assistant);
  g_signal_connect (G_OBJECT (assistant->apply), "clicked",
		    G_CALLBACK (on_assistant_apply), assistant);
  g_signal_connect (G_OBJECT (assistant->forward), "clicked",
		    G_CALLBACK (on_assistant_forward), assistant);
  g_signal_connect (G_OBJECT (assistant->back), "clicked",
		    G_CALLBACK (on_assistant_back), assistant);
  g_signal_connect (G_OBJECT (assistant->cancel), "clicked",
		    G_CALLBACK (on_assistant_cancel), assistant);
  g_signal_connect (G_OBJECT (assistant->last), "clicked",
		    G_CALLBACK (on_assistant_last), assistant);
}

static void
gtk_assistant_set_child_property (GtkContainer    *container,
				  GtkWidget       *child,
				  guint            property_id,
				  const GValue    *value,
				  GParamSpec      *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_PAGE_TYPE:
      gtk_assistant_set_page_type (GTK_ASSISTANT (container), child,
				   g_value_get_enum (value));
      break;
    case CHILD_PROP_PAGE_TITLE:
      gtk_assistant_set_page_title (GTK_ASSISTANT (container), child,
				    g_value_get_string (value));
      break;
    case CHILD_PROP_PAGE_HEADER_IMAGE:
      gtk_assistant_set_page_header_image (GTK_ASSISTANT (container), child,
					   g_value_get_object (value));
      break;
    case CHILD_PROP_PAGE_SIDEBAR_IMAGE:
      gtk_assistant_set_page_side_image (GTK_ASSISTANT (container), child,
					 g_value_get_object (value));
      break;
    case CHILD_PROP_PAGE_COMPLETE:
      gtk_assistant_set_page_complete (GTK_ASSISTANT (container), child,
				       g_value_get_boolean (value));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
gtk_assistant_get_child_property (GtkContainer *container,
				  GtkWidget    *child,
				  guint         property_id,
				  GValue       *value,
				  GParamSpec   *pspec)
{
  switch (property_id)
    {
    case CHILD_PROP_PAGE_TYPE:
      g_value_set_enum (value,
			gtk_assistant_get_page_type (GTK_ASSISTANT (container), child));
      break;
    case CHILD_PROP_PAGE_TITLE:
      g_value_set_string (value,
			  gtk_assistant_get_page_title (GTK_ASSISTANT (container), child));
      break;
    case CHILD_PROP_PAGE_HEADER_IMAGE:
      g_value_set_object (value,
			  gtk_assistant_get_page_header_image (GTK_ASSISTANT (container), child));
      break;
    case CHILD_PROP_PAGE_SIDEBAR_IMAGE:
      g_value_set_object (value,
			  gtk_assistant_get_page_side_image (GTK_ASSISTANT (container), child));
      break;
    case CHILD_PROP_PAGE_COMPLETE:
      g_value_set_boolean (value,
			   gtk_assistant_get_page_complete (GTK_ASSISTANT (container), child));
      break;
    default:
      GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
      break;
    }
}

static void
remove_page (GtkAssistant *assistant, 
	     GList        *element)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GtkAssistantPage *page_info;

  page_info = element->data;

  /* If we are mapped and visible, we want to deal with changing the page. */
  if ((GTK_WIDGET_MAPPED (page_info->page)) && (page_info == priv->current_page))
    compute_next_step (assistant);

  priv->pages = g_list_remove_link (priv->pages, element);

  priv->visited_pages = g_slist_remove_all (priv->visited_pages, page_info);
  gtk_widget_unparent (page_info->page);

  if (page_info->header_image)
    g_object_unref (page_info->header_image);

  if (page_info->sidebar_image)
    g_object_unref (page_info->sidebar_image);

  gtk_widget_destroy (page_info->title);
  g_free (page_info);
  g_list_free_1 (element);
}

static void
gtk_assistant_destroy (GtkObject *object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);
  GtkAssistantPrivate *priv = assistant->priv;

  if (priv->header_image)
    {
      gtk_widget_destroy (priv->header_image);
      priv->header_image = NULL;
    }

  if (priv->sidebar_image)
    {
      gtk_widget_destroy (priv->sidebar_image);
      priv->sidebar_image = NULL;
    }

  if (priv->action_area)
    {
      gtk_widget_destroy (priv->action_area);
      priv->action_area = NULL;
    }

  if (priv->size_group)
    {
      g_object_unref (priv->size_group);
      priv->size_group = NULL;
    }

  if (priv->forward_function)
    {
      if (priv->forward_function_data &&
	  priv->forward_data_destroy)
	priv->forward_data_destroy (priv->forward_function_data);

      priv->forward_function = NULL;
      priv->forward_function_data = NULL;
      priv->forward_data_destroy = NULL;
    }

  if (priv->visited_pages)
    {
      g_slist_free (priv->visited_pages);
      priv->visited_pages = NULL;
    }

  /* We set current to NULL so that the remove code doesn't try
   * to do anything funny */
  priv->current_page = NULL;

  while (priv->pages)
    remove_page (GTK_ASSISTANT (object), priv->pages);
      
  GTK_OBJECT_CLASS (gtk_assistant_parent_class)->destroy (object);
}

static GList*
find_page (GtkAssistant  *assistant,
	   GtkWidget     *page)
{
  GtkAssistantPrivate *priv = assistant->priv;
  GList *child = priv->pages;
  
  while (child)
    {
      GtkAssistantPage *page_info = child->data;
      if (page_info->page == page)
	return child;

      child = child->next;
    }
  
  return NULL;
}

static void
set_title_colors (GtkWidget *assistant,
		  GtkWidget *title_label)
{
  GtkStyle *style;

  gtk_widget_ensure_style (assistant);
  style = gtk_widget_get_style (assistant);

  /* change colors schema, for making the header text visible */
  gtk_widget_modify_bg (title_label, GTK_STATE_NORMAL, &style->bg[GTK_STATE_SELECTED]);
  gtk_widget_modify_fg (title_label, GTK_STATE_NORMAL, &style->fg[GTK_STATE_SELECTED]);
}

static void
set_title_font (GtkWidget *assistant,
		GtkWidget *title_label)
{
  PangoFontDescription *desc;
  gint size;

  desc = pango_font_description_new ();
  size = pango_font_description_get_size (assistant->style->font_desc);

  pango_font_description_set_weight (desc, PANGO_WEIGHT_ULTRABOLD);
  pango_font_description_set_size   (desc, size * PANGO_SCALE_XX_LARGE);

  gtk_widget_modify_font (title_label, desc);
  pango_font_description_free (desc);
}

static void
gtk_assistant_style_set (GtkWidget *widget,
			 GtkStyle  *old_style)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GList *list;

  list = priv->pages;

  while (list)
    {
      GtkAssistantPage *page = list->data;

      set_title_colors (widget, page->title);
      set_title_font (widget, page->title);

      list = list->next;
    }
}

static void
gtk_assistant_size_request (GtkWidget      *widget,
			    GtkRequisition *requisition)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GtkRequisition child_requisition;
  gint header_padding, content_padding;
  gint width, height, header_width, header_height;
  GList *list;

  gtk_widget_style_get (widget,
			"header-padding", &header_padding,
			"content-padding", &content_padding,
			NULL);
  width = height = 0;
  header_width = header_height = 0;
  list  = priv->pages;

  while (list)
    {
      GtkAssistantPage *page = list->data;
      gint w, h;

      gtk_widget_size_request (page->page, &child_requisition);
      width  = MAX (width,  child_requisition.width);
      height = MAX (height, child_requisition.height);

      gtk_widget_size_request (page->title, &child_requisition);
      w = child_requisition.width;
      h = child_requisition.height;

      if (page->header_image)
	{
	  w += gdk_pixbuf_get_width (page->header_image) + HEADER_SPACING;
	  h  = MAX (h, gdk_pixbuf_get_height (page->header_image));
	}

      header_width  = MAX (header_width, w);
      header_height = MAX (header_height, h);

      list = list->next;
    }

  gtk_widget_size_request (priv->sidebar_image, &child_requisition);
  width  += child_requisition.width;
  height  = MAX (height, child_requisition.height);

  gtk_widget_set_size_request (priv->header_image, header_width, header_height);
  gtk_widget_size_request (priv->header_image, &child_requisition);
  width   = MAX (width, header_width) + 2 * header_padding;
  height += header_height + 2 * header_padding;

  gtk_widget_size_request (priv->action_area, &child_requisition);
  width   = MAX (width, child_requisition.width);
  height += child_requisition.height;

  width += GTK_CONTAINER (widget)->border_width * 2 + content_padding * 2;
  height += GTK_CONTAINER (widget)->border_width * 2 + content_padding * 2;

  requisition->width = width;
  requisition->height = height;
}

static void
gtk_assistant_size_allocate (GtkWidget      *widget,
			     GtkAllocation  *allocation)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GtkRequisition header_requisition;
  GtkAllocation child_allocation, header_allocation;
  gint header_padding, content_padding;
  gboolean rtl;
  GList *pages;

  rtl   = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);
  pages = priv->pages;

  gtk_widget_style_get (widget,
			"header-padding", &header_padding,
			"content-padding", &content_padding,
			NULL);

  widget->allocation = *allocation;

  /* Header */
  gtk_widget_get_child_requisition (priv->header_image, &header_requisition);

  header_allocation.x = allocation->x + GTK_CONTAINER (widget)->border_width + header_padding;
  header_allocation.y = allocation->y + GTK_CONTAINER (widget)->border_width + header_padding;
  header_allocation.width  = allocation->width - 2 * GTK_CONTAINER (widget)->border_width - 2 * header_padding;
  header_allocation.height = header_requisition.height;

  gtk_widget_size_allocate (priv->header_image, &header_allocation);

  /* Action area */
  child_allocation.x = allocation->x + GTK_CONTAINER (widget)->border_width;
  child_allocation.y = allocation->y + allocation->height -
    GTK_CONTAINER (widget)->border_width - priv->action_area->requisition.height;
  child_allocation.width  = allocation->width - 2 * GTK_CONTAINER (widget)->border_width;
  child_allocation.height = priv->action_area->requisition.height;

  gtk_widget_size_allocate (priv->action_area, &child_allocation);

  /* Sidebar */
  if (rtl)
    child_allocation.x = allocation->x + allocation->width -
      GTK_CONTAINER (widget)->border_width - priv->sidebar_image->requisition.width;
  else
    child_allocation.x = allocation->x + GTK_CONTAINER (widget)->border_width;

  child_allocation.y = allocation->y + GTK_CONTAINER (widget)->border_width +
    priv->header_image->allocation.height + 2 * header_padding;
  child_allocation.width = priv->sidebar_image->requisition.width;
  child_allocation.height = allocation->height - 2 * GTK_CONTAINER (widget)->border_width -
    priv->header_image->allocation.height - 2 * header_padding - priv->action_area->allocation.height;

  gtk_widget_size_allocate (priv->sidebar_image, &child_allocation);

  /* Pages */
  child_allocation.x = allocation->x + GTK_CONTAINER (widget)->border_width + content_padding;
  child_allocation.y = allocation->y + GTK_CONTAINER (widget)->border_width +
    priv->header_image->allocation.height + 2 * header_padding + content_padding;
  child_allocation.width  = allocation->width - 2 * GTK_CONTAINER (widget)->border_width - 2 * content_padding;
  child_allocation.height = allocation->height - 2 * GTK_CONTAINER (widget)->border_width -
    priv->header_image->allocation.height - 2 * header_padding - priv->action_area->allocation.height - 2 * content_padding;

  if (GTK_WIDGET_VISIBLE (priv->sidebar_image))
    {
      if (!rtl)
	child_allocation.x += priv->sidebar_image->allocation.width;

      child_allocation.width -= priv->sidebar_image->allocation.width;
    }

  while (pages)
    {
      GtkAssistantPage *page = pages->data;

      gtk_widget_size_allocate (page->page, &child_allocation);
      gtk_widget_size_allocate (page->title, &header_allocation);
      pages = pages->next;
    }
}

static void
gtk_assistant_map (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GList *page_node;

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

  gtk_widget_map (priv->header_image);
  gtk_widget_map (priv->action_area);

  if (GTK_WIDGET_VISIBLE (priv->sidebar_image) &&
      !GTK_WIDGET_MAPPED (priv->sidebar_image))
    gtk_widget_map (priv->sidebar_image);

  /* if there's no default page, pick the first one */
  if (!priv->current_page && priv->pages)
    {
      page_node = priv->pages;

      while (!GTK_WIDGET_VISIBLE (((GtkAssistantPage *) page_node->data)->page))
	page_node = page_node->next;

      if (page_node)
	priv->current_page = page_node->data;
    }

  if (priv->current_page &&
      GTK_WIDGET_VISIBLE (priv->current_page->page) &&
      !GTK_WIDGET_MAPPED (priv->current_page->page))
    {
      _set_assistant_buttons_state ((GtkAssistant *) widget);
      _set_assistant_header_image ((GtkAssistant*) widget);
      _set_assistant_sidebar_image ((GtkAssistant*) widget);

      g_signal_emit (widget, signals [PREPARE], 0, priv->current_page->page);
      gtk_widget_map (priv->current_page->page);
      gtk_widget_map (priv->current_page->title);
    }

  GTK_WIDGET_CLASS (gtk_assistant_parent_class)->map (widget);
}

static void
gtk_assistant_unmap (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  gtk_widget_unmap (priv->header_image);
  gtk_widget_unmap (priv->action_area);

  if (GTK_WIDGET_DRAWABLE (priv->sidebar_image))
    gtk_widget_unmap (priv->sidebar_image);

  if (priv->current_page &&
      GTK_WIDGET_DRAWABLE (priv->current_page->page))
    gtk_widget_unmap (priv->current_page->page);

  g_slist_free (priv->visited_pages);
  priv->visited_pages = NULL;
  priv->current_page  = NULL;

  GTK_WIDGET_CLASS (gtk_assistant_parent_class)->unmap (widget);
}

static gboolean
gtk_assistant_delete_event (GtkWidget   *widget,
			    GdkEventAny *event)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;

  /* Do not allow cancelling in the middle of a progress page */
  if (priv->current_page &&
      (priv->current_page->type != GTK_ASSISTANT_PAGE_PROGRESS ||
       priv->current_page->complete))
    g_signal_emit (widget, signals [CANCEL], 0, NULL);

  return TRUE;
}

static void
assistant_paint_colored_box (GtkWidget *widget)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  gint header_padding, content_padding;
  cairo_t *cr;
  gint content_x, content_width;
  gboolean rtl;

  cr   = gdk_cairo_create (widget->window);
  rtl  = (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL);

  gtk_widget_style_get (widget,
			"header-padding",  &header_padding,
			"content-padding", &content_padding,
			NULL);

  /* colored box */
  gdk_cairo_set_source_color (cr, &widget->style->bg[GTK_STATE_SELECTED]);
  cairo_rectangle (cr,
		   0, 0,
		   widget->allocation.width,
		   widget->allocation.height - priv->action_area->allocation.height);
  cairo_fill (cr);

  /* content box */
  content_x = content_padding;
  content_width = widget->allocation.width - 2 * content_padding;

  if (GTK_WIDGET_VISIBLE (priv->sidebar_image))
    {
      if (!rtl)
	content_x += priv->sidebar_image->allocation.width;
      content_width -= priv->sidebar_image->allocation.width;
    }
  
  gdk_cairo_set_source_color (cr, &widget->style->bg[GTK_STATE_NORMAL]);

  cairo_rectangle (cr,
		   content_x,
		   priv->header_image->allocation.height + content_padding + 2 * header_padding,
		   content_width,
		   widget->allocation.height - priv->action_area->allocation.height -
		   priv->header_image->allocation.height - 2 * content_padding - 2 * header_padding);
  cairo_fill (cr);

  cairo_destroy (cr);
}

static gboolean
gtk_assistant_expose (GtkWidget      *widget,
		      GdkEventExpose *event)
{
  GtkAssistant *assistant = GTK_ASSISTANT (widget);
  GtkAssistantPrivate *priv = assistant->priv;
  GtkContainer *container;

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      container = GTK_CONTAINER (widget);

      assistant_paint_colored_box (widget);

      gtk_container_propagate_expose (container, priv->header_image, event);
      gtk_container_propagate_expose (container, priv->sidebar_image, event);
      gtk_container_propagate_expose (container, priv->action_area, event);

      if (priv->current_page)
	{
	  gtk_container_propagate_expose (container, priv->current_page->page, event);
	  gtk_container_propagate_expose (container, priv->current_page->title, event);
	}
    }

  return FALSE;
}

static void
gtk_assistant_add (GtkContainer *container,
		   GtkWidget    *page)
{
  g_return_if_fail (GTK_IS_WIDGET (page));

  gtk_assistant_append_page (GTK_ASSISTANT (container), page);
}

static void
gtk_assistant_remove (GtkContainer *container,
		      GtkWidget    *page)
{
  GtkAssistant *assistant;
  GList *element;

  assistant = (GtkAssistant*) container;

  element = find_page (assistant, page);

  if (element)
    {
      remove_page (assistant, element);
      gtk_widget_queue_resize ((GtkWidget *) container);
    }
}

static void
gtk_assistant_forall (GtkContainer *container,
		      gboolean      include_internals,
		      GtkCallback   callback,
		      gpointer      callback_data)
{
  GtkAssistant *assistant = (GtkAssistant*) container;
  GtkAssistantPrivate *priv = assistant->priv;
  GList *pages;

  if (include_internals)
    {
      (*callback) (priv->header_image, callback_data);
      (*callback) (priv->sidebar_image, callback_data);
      (*callback) (priv->action_area, callback_data);
    }

  pages = priv->pages;

  while (pages)
    {
      GtkAssistantPage *page = (GtkAssistantPage *) pages->data;

      (*callback) (page->page, callback_data);

      if (include_internals)
	(*callback) (page->title, callback_data);

      pages = pages->next;
    }
}

/**
 * gtk_assistant_new:
 * 
 * Creates a new #GtkAssistant.
 *
 * Return value: a newly created #GtkAssistant
 *
 * Since: 2.10
 **/
GtkWidget*
gtk_assistant_new (void)
{
  GtkWidget *assistant;

  assistant = g_object_new (GTK_TYPE_ASSISTANT, NULL);

  return assistant;
}

/**
 * gtk_assistant_get_current_page:
 * @assistant: a #GtkAssistant
 *
 * Returns the page number of the current page
 *
 * Return value: The index (starting from 0) of the current page in
 * the @assistant, if the @assistant has no pages, -1 will be returned
 *
 * Since: 2.10
 **/
gint
gtk_assistant_get_current_page (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), -1);

  priv = assistant->priv;

  if (!priv->pages || !priv->current_page)
    return -1;

  return g_list_index (priv->pages, priv->current_page);
}

/**
 * gtk_assistant_set_current_page:
 * @assistant: a #GtkAssistant
 * @page_num: index of the page to switch to, starting from 0.
 *            If negative, the last page will be used. If greater
 *            than the number of pages in the @assistant, nothing
 *            will be done.
 *
 * Switches the page to @page_num. Note that this will only be necessary
 * in custom buttons, as the @assistant flow can be set with
 * gtk_assistant_set_forward_page_func().
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_current_page (GtkAssistant *assistant,
				gint          page_num)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  priv = assistant->priv;

  if (page_num >= 0)
    page = (GtkAssistantPage *) g_list_nth_data (priv->pages, page_num);
  else
    page = (GtkAssistantPage *) g_list_last (priv->pages);

  g_return_if_fail (page != NULL);

  if (priv->current_page == page)
    return;

  /* only add the page to the visited list if the
   * assistant is mapped, if not, just use it as an
   * initial page setting, for the cases where the
   * initial page is != to 0
   */
  if (GTK_WIDGET_MAPPED (assistant))
    priv->visited_pages = g_slist_prepend (priv->visited_pages, page);

  _set_current_page (assistant, page);
}

/**
 * gtk_assistant_get_n_pages:
 * @assistant: a #GtkAssistant
 *
 * Returns the number of pages in the @assistant
 *
 * Return value: The number of pages in the @assistant.
 *
 * Since: 2.10
 **/
gint
gtk_assistant_get_n_pages (GtkAssistant *assistant)
{
  GtkAssistantPrivate *priv;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);

  priv = assistant->priv;

  return g_list_length (priv->pages);
}

/**
 * gtk_assistant_get_nth_page:
 * @assistant: a #GtkAssistant
 * @page_num: The index of a page in the @assistant, or -1 to get the last page;
 *
 * Returns the child widget contained in page number @page_num.
 *
 * Return value: The child widget, or %NULL if @page_num is out of bounds.
 *
 * Since: 2.10
 **/
GtkWidget*
gtk_assistant_get_nth_page (GtkAssistant *assistant,
			    gint          page_num)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page;
  GList *elem;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);

  priv = assistant->priv;

  elem = g_list_nth (priv->pages, page_num);

  if (!elem)
    return NULL;

  page = (GtkAssistantPage *) elem->data;

  return page->page;
}

/**
 * gtk_assistant_prepend_page:
 * @assistant: a #GtkAssistant
 * @page: a #GtkWidget
 *
 * Prepends a page to the @assistant.
 *
 * Return value: the index (starting at 0) of the inserted page
 *
 * Since: 2.10
 **/
gint
gtk_assistant_prepend_page (GtkAssistant *assistant,
			    GtkWidget    *page)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);

  return gtk_assistant_insert_page (assistant, page, 0);
}

/**
 * gtk_assistant_append_page:
 * @assistant: a #GtkAssistant
 * @page: a #GtkWidget
 *
 * Appends a page to the @assistant.
 *
 * Return value: the index (starting at 0) of the inserted page
 *
 * Since: 2.10
 **/
gint
gtk_assistant_append_page (GtkAssistant *assistant,
			   GtkWidget    *page)
{
  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);

  return gtk_assistant_insert_page (assistant, page, -1);
}

/**
 * gtk_assistant_insert_page:
 * @assistant: a #GtkAssistant
 * @page: a #GtkWidget
 * @position: the index (starting at 0) at which to insert the page,
 *            or -1 to append the page to the @assistant
 *
 * Inserts a page in the @assistant at a given position.
 *
 * Return value: the index (starting from 0) of the inserted page
 *
 * Since: 2.10
 **/
gint
gtk_assistant_insert_page (GtkAssistant *assistant,
			   GtkWidget    *page,
			   gint          position)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  gint n_pages;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), 0);
  g_return_val_if_fail (GTK_IS_WIDGET (page), 0);
  g_return_val_if_fail (page->parent == NULL, 0);
  g_return_val_if_fail (!GTK_WIDGET_TOPLEVEL (page), 0);

  priv = assistant->priv;

  page_info = g_new0 (GtkAssistantPage, 1);
  page_info->page  = page;
  page_info->title = gtk_label_new (NULL);

  gtk_misc_set_alignment (GTK_MISC (page_info->title), 0.,0.5);
  set_title_colors (GTK_WIDGET (assistant), page_info->title);
  set_title_font   (GTK_WIDGET (assistant), page_info->title);
  gtk_widget_show  (page_info->title);

  n_pages = g_list_length (priv->pages);

  if (position < 0 || position > n_pages)
    position = n_pages;

  priv->pages = g_list_insert (priv->pages, page_info, position);

  gtk_widget_set_parent (page_info->page,  GTK_WIDGET (assistant));
  gtk_widget_set_parent (page_info->title, GTK_WIDGET (assistant));

  if (GTK_WIDGET_REALIZED (GTK_WIDGET (assistant)))
    {
      gtk_widget_realize (page_info->page);
      gtk_widget_realize (page_info->title);
    }

  gtk_widget_queue_resize (GTK_WIDGET (assistant));

  return position;
}

/**
 * gtk_assistant_set_forward_page_func:
 * @assistant: a #GtkAssistant
 * @page_func: the #GtkAssistantPageFunc, or %NULL to use the default one
 * @data: user data for @page_func
 * @destroy: destroy notifier for @data
 *
 * Sets the page forwarding function to be @page_func, this function will
 * be used to determine what will be the next page when the user presses
 * the forward button. Setting @page_func to %NULL will make the assistant
 * to use the default forward function, which just goes to the next visible 
 * page.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_forward_page_func (GtkAssistant         *assistant,
				     GtkAssistantPageFunc  page_func,
				     gpointer              data,
				     GDestroyNotify        destroy)
{
  GtkAssistantPrivate *priv;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));

  priv = assistant->priv;

  if (priv->forward_data_destroy &&
      priv->forward_function_data)
    (*priv->forward_data_destroy) (priv->forward_function_data);

  if (page_func)
    {
      priv->forward_function = page_func;
      priv->forward_function_data = data;
      priv->forward_data_destroy = destroy;
    }
  else
    {
      priv->forward_function = default_forward_function;
      priv->forward_function_data = assistant;
      priv->forward_data_destroy = NULL;
    }
}

/**
 * gtk_assistant_add_action_widget:
 * @dialog: a #GtkAssistant
 * @child: a #GtkWidget
 * 
 * Adds a widget to the action area of a #GtkAssistant.
 *
 * Since: 2.10
 **/
void
gtk_assistant_add_action_widget (GtkAssistant *assistant,
				 GtkWidget    *child)
{
  GtkAssistantPrivate *priv;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = assistant->priv;

  if (GTK_IS_BUTTON (child))
    gtk_size_group_add_widget (priv->size_group, child);

  gtk_box_pack_end (GTK_BOX (priv->action_area), child, FALSE, FALSE, 0);
}

/**
 * gtk_assistant_remove_action_widget:
 * @dialog: a #GtkAssistant
 * @child: a #GtkWidget
 *
 * Removes a widget from the action area of a #GtkAssistant.
 *
 * Since: 2.10
 **/
void
gtk_assistant_remove_action_widget (GtkAssistant *assistant,
				    GtkWidget    *child)
{
  GtkAssistantPrivate *priv;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (child));

  priv = assistant->priv;

  if (GTK_IS_BUTTON (child))
    gtk_size_group_remove_widget (priv->size_group, child);

  gtk_container_remove (GTK_CONTAINER (priv->action_area), child);
}

/**
 * gtk_assistant_set_page_title:
 * @assistant: a #GtkAssistant
 * @page: a page of @assitant
 * @title: the new title for @page
 * 
 * Sets a title for @page. The title is displayed in the header
 * area of the assistant when @page is the current page.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_title (GtkAssistant *assistant,
			      GtkWidget    *page,
			      const gchar  *title)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  gtk_label_set_text ((GtkLabel*) page_info->title, title);
  gtk_widget_queue_resize (GTK_WIDGET (assistant));
  gtk_widget_child_notify (page, "title");
}

/**
 * gtk_assistant_get_page_title:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * 
 * Gets the title for @page. 
 * 
 * Return value: the title for @page.
 *
 * Since: 2.10
 **/
G_CONST_RETURN gchar*
gtk_assistant_get_page_title (GtkAssistant *assistant,
			      GtkWidget    *page)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return gtk_label_get_text ((GtkLabel*) page_info->title);
}

/**
 * gtk_assistant_set_page_type:
 * @assistant: a #GtkAssistant
 * @page: a page of @assitant
 * @type: the new type for @page
 * 
 * Sets the page type for @page. The page type determines the page
 * behavior in the @assistant.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_type (GtkAssistant         *assistant,
			     GtkWidget            *page,
			     GtkAssistantPageType  type)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (type != page_info->type)
    {
      page_info->type = type;

      /* Always set buttons state, a change in a future page
	 might change current page buttons */
      if (priv->current_page)
	_set_assistant_buttons_state (assistant);

      gtk_widget_child_notify (page, "page-type");
    }
}

/**
 * gtk_assistant_get_page_type:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 *
 * Gets the page type of @page.
 *
 * Return value: the page type of @page.
 *
 * Since: 2.10
 **/
GtkAssistantPageType
gtk_assistant_get_page_type (GtkAssistant *assistant,
			     GtkWidget    *page)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), GTK_ASSISTANT_PAGE_CONTENT);
  g_return_val_if_fail (GTK_IS_WIDGET (page), GTK_ASSISTANT_PAGE_CONTENT);

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, GTK_ASSISTANT_PAGE_CONTENT);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->type;
}

/**
 * gtk_assistant_set_page_header_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assitant
 * @pixbuf: the new header image @page
 * 
 * Sets a header image for @page. This image is displayed in the header
 * area of the assistant when @page is the current page.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_header_image (GtkAssistant *assistant,
				     GtkWidget    *page,
				     GdkPixbuf    *pixbuf)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (pixbuf != page_info->header_image)
    {
      if (page_info->header_image)
	{
	  g_object_unref (page_info->header_image);
	  page_info->header_image = NULL;
	}

      if (pixbuf)
	page_info->header_image = g_object_ref (pixbuf);

      if (page_info == priv->current_page)
	_set_assistant_header_image (assistant);

      gtk_widget_child_notify (page, "header-image");
    }
}

/**
 * gtk_assistant_get_page_header_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * 
 * Gets the header image for @page. 
 * 
 * Return value: the header image for @page, or %NULL
 * if there's no header image for the page.
 *
 * Since: 2.10
 **/
GdkPixbuf*
gtk_assistant_get_page_header_image (GtkAssistant *assistant,
				     GtkWidget    *page)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->header_image;
}

/**
 * gtk_assistant_set_page_side_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assitant
 * @pixbuf: the new header image @page
 * 
 * Sets a header image for @page. This image is displayed in the side
 * area of the assistant when @page is the current page.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_side_image (GtkAssistant *assistant,
				   GtkWidget    *page,
				   GdkPixbuf    *pixbuf)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (pixbuf != page_info->sidebar_image)
    {
      if (page_info->sidebar_image)
	{
	  g_object_unref (page_info->sidebar_image);
	  page_info->sidebar_image = NULL;
	}

      if (pixbuf)
	page_info->sidebar_image = g_object_ref (pixbuf);

      if (page_info == priv->current_page)
	_set_assistant_sidebar_image (assistant);

      gtk_widget_child_notify (page, "sidebar-image");
    }
}

/**
 * gtk_assistant_get_page_side_image:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * 
 * Gets the header image for @page. 
 * 
 * Return value: the side image for @page, or %NULL
 * if there's no side image for the page.
 *
 * Since: 2.10
 **/
GdkPixbuf*
gtk_assistant_get_page_side_image (GtkAssistant *assistant,
				   GtkWidget    *page)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), NULL);
  g_return_val_if_fail (GTK_IS_WIDGET (page), NULL);

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, NULL);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->sidebar_image;
}

/**
 * gtk_assistant_set_page_complete:
 * @assistant: a #GtkAssistant
 * @page: a page of @assitant
 * @pixbuf: the new header image @page
 * 
 * Sets whether @page contents are complete. This will make
 * @assistant update the buttons state to be able to continue the task.
 *
 * Since: 2.10
 **/
void
gtk_assistant_set_page_complete (GtkAssistant *assistant,
				 GtkWidget    *page,
				 gboolean      complete)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_if_fail (GTK_IS_ASSISTANT (assistant));
  g_return_if_fail (GTK_IS_WIDGET (page));

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_if_fail (child != NULL);

  page_info = (GtkAssistantPage*) child->data;

  if (complete != page_info->complete)
    {
      page_info->complete = complete;

      /* Always set buttons state, a change in a future page
	 might change current page buttons */
      if (priv->current_page)
	{
	  /* Always set buttons state, a change in a future page
	     might change current page buttons */
	  _set_assistant_buttons_state (assistant);
	}

      gtk_widget_child_notify (page, "complete");
    }
}

/**
 * gtk_assistant_get_page_complete:
 * @assistant: a #GtkAssistant
 * @page: a page of @assistant
 * 
 * Gets whether @page is complete..
 * 
 * Return value: %TRUE if @page is complete.
 *
 * Since: 2.10
 **/
gboolean
gtk_assistant_get_page_complete (GtkAssistant *assistant,
				 GtkWidget    *page)
{
  GtkAssistantPrivate *priv;
  GtkAssistantPage *page_info;
  GList *child;

  g_return_val_if_fail (GTK_IS_ASSISTANT (assistant), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (page), FALSE);

  priv = assistant->priv;
  child = find_page (assistant, page);

  g_return_val_if_fail (child != NULL, FALSE);

  page_info = (GtkAssistantPage*) child->data;

  return page_info->complete;
}


#define __GTK_ASSISTANT_C__
#include "gtkaliasdef.c"
