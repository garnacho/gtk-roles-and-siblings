/* gtktoolbutton.h
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@codefactory.se>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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

#ifndef __GTK_TOOL_BUTTON_H__
#define __GTK_TOOL_BUTTON_H__

#include "gtktoolitem.h"

G_BEGIN_DECLS

#define GTK_TYPE_TOOL_BUTTON            (gtk_tool_button_get_type ())
#define GTK_TOOL_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TOOL_BUTTON, GtkToolButton))
#define GTK_TOOL_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_TOOL_BUTTON, GtkToolButtonClass))
#define GTK_IS_TOOL_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TOOL_BUTTON))
#define GTK_IS_TOOL_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), GTK_TYPE_TOOL_BUTTON))
#define GTK_TOOL_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_TOOL_BUTTON, GtkToolButtonClass))

typedef struct _GtkToolButton        GtkToolButton;
typedef struct _GtkToolButtonClass   GtkToolButtonClass;
typedef struct _GtkToolButtonPrivate GtkToolButtonPrivate;

struct _GtkToolButton
{
  GtkToolItem parent;

  GtkWidget *button;

  gchar *stock_id;
  gchar *label_text;
  GtkWidget *label_widget;
  GtkWidget *icon_widget;
  GtkIconSet *icon_set;
  
  guint use_underline : 1;
};

struct _GtkToolButtonClass
{
  GtkToolItemClass parent_class;
 
  GType button_type;

  /* signal */
  void       (* clicked)             (GtkToolButton    *tool_item);
};

GType        gtk_tool_button_get_type       (void);
GtkToolItem *gtk_tool_button_new            (void);
GtkToolItem *gtk_tool_button_new_from_stock (const gchar *stock_id);

void                  gtk_tool_button_set_label       (GtkToolButton *button,
						       const gchar   *label);
G_CONST_RETURN gchar *gtk_tool_button_get_label       (GtkToolButton *button);
void                  gtk_tool_button_set_use_underline (GtkToolButton *button,
							 gboolean       use_underline);
gboolean              gtk_tool_button_get_use_underline (GtkToolButton *button);
void		      gtk_tool_button_set_stock_id      (GtkToolButton *button,
							 const gchar   *stock_id);
G_CONST_RETURN gchar *gtk_tool_button_get_stock_id      (GtkToolButton *button);
void		      gtk_tool_button_set_icon_set      (GtkToolButton *button,
							 GtkIconSet    *icon_set);
GtkIconSet *          gtk_tool_button_get_icon_set      (GtkToolButton *button);
void                  gtk_tool_button_set_icon_widget (GtkToolButton *button,
						       GtkWidget     *icon);
GtkWidget *           gtk_tool_button_get_icon_widget (GtkToolButton *button);

void                  gtk_tool_button_set_label_widget (GtkToolButton *button,
							GtkWidget     *label_widget);
GtkWidget *           gtk_tool_button_get_label_widget (GtkToolButton *button);

G_END_DECLS

#endif /* __GTK_TOOL_BUTTON_H__ */
