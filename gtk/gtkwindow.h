/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_WINDOW_H__
#define __GTK_WINDOW_H__


#include <gdk/gdk.h>
#include <gtk/gtkaccelgroup.h>
#include <gtk/gtkbin.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkwidget.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_WINDOW			(gtk_window_get_type ())
#define GTK_WINDOW(obj)			(GTK_CHECK_CAST ((obj), GTK_TYPE_WINDOW, GtkWindow))
#define GTK_WINDOW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_WINDOW, GtkWindowClass))
#define GTK_IS_WINDOW(obj)		(GTK_CHECK_TYPE ((obj), GTK_TYPE_WINDOW))
#define GTK_IS_WINDOW_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WINDOW))
#define GTK_WINDOW_GET_CLASS(obj)       (GTK_CHECK_GET_CLASS ((obj), GTK_TYPE_WINDOW, GtkWindowClass))


typedef struct _GtkWindow             GtkWindow;
typedef struct _GtkWindowClass        GtkWindowClass;
typedef struct _GtkWindowGeometryInfo GtkWindowGeometryInfo;
typedef struct _GtkWindowGroup        GtkWindowGroup;
typedef struct _GtkWindowGroupClass   GtkWindowGroupClass;

struct _GtkWindow
{
  GtkBin bin;

  gchar *title;
  gchar *wmclass_name;
  gchar *wmclass_class;
  gchar *wm_role;

  GtkWidget *focus_widget;
  GtkWidget *default_widget;
  GtkWindow *transient_parent;
  GtkWindowGeometryInfo *geometry_info;
  GdkWindow *frame;
  GtkWindowGroup *group;

  guint16 configure_request_count;
  guint allow_shrink : 1;
  guint allow_grow : 1;
  guint configure_notify_received : 1;
  /* The following flags are initially TRUE (before a window is mapped).
   * They cause us to compute a configure request that involves
   * default-only parameters. Once mapped, we set them to FALSE.
   * Then we set them to TRUE again on unmap (for position)
   * and on unrealize (for size).
   */
  guint need_default_position : 1;
  guint need_default_size : 1;
  guint position : 3;
  GtkWindowType type : 4;
  guint has_user_ref_count : 1;
  guint has_focus : 1;

  guint modal : 1;
  guint destroy_with_parent : 1;
  
  guint has_frame : 1;

  /* gtk_window_iconify() called before realization */
  guint iconify_initially : 1;
  guint stick_initially : 1;
  guint maximize_initially : 1;
  guint decorated : 1;
  
  GdkWindowTypeHint type_hint : 3;
  GdkGravity gravity : 5;
  
  guint frame_left;
  guint frame_top;
  guint frame_right;
  guint frame_bottom;
  
  GdkModifierType mnemonic_modifier;
  gpointer gtk_reserved1;	/* For future GdkScreen * */
};

struct _GtkWindowClass
{
  GtkBinClass parent_class;

  void     (* set_focus)   (GtkWindow *window,
			    GtkWidget *focus);
  gboolean (* frame_event) (GtkWidget *widget,
			    GdkEvent  *event);

  /* G_SIGNAL_ACTION signals for keybindings */

  void     (* activate_focus)          (GtkWindow       *window);
  void     (* activate_default)        (GtkWindow       *window);
  void     (* move_focus)              (GtkWindow       *window,
                                        GtkDirectionType direction);  
};

#define GTK_TYPE_WINDOW_GROUP             (gtk_window_group_get_type ())
#define GTK_WINDOW_GROUP(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), GTK_TYPE_WINDOW_GROUP, GtkWindowGroup))
#define GTK_WINDOW_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_WINDOW_GROUP, GtkWindowGroupClass))
#define GTK_IS_WINDOW_GROUP(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), GTK_TYPE_WINDOW_GROUP))
#define GTK_IS_WINDOW_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_WINDOW_GROUP))
#define GTK_WINDOW_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_WINDOW_GROUP, GtkWindowGroupClass))

struct _GtkWindowGroup
{
  GObject parent_instance;

  GSList *grabs;
};

struct _GtkWindowGroupClass
{
  GObjectClass parent_class;
};

GtkType    gtk_window_get_type                 (void) G_GNUC_CONST;
GtkWidget* gtk_window_new                      (GtkWindowType        type);
void       gtk_window_set_title                (GtkWindow           *window,
						const gchar         *title);
G_CONST_RETURN gchar *gtk_window_get_title     (GtkWindow           *window);
void       gtk_window_set_wmclass              (GtkWindow           *window,
						const gchar         *wmclass_name,
						const gchar         *wmclass_class);
void       gtk_window_set_role                 (GtkWindow           *window,
                                                const gchar         *role);
G_CONST_RETURN gchar *gtk_window_get_role      (GtkWindow           *window);
void       gtk_window_add_accel_group          (GtkWindow           *window,
						GtkAccelGroup	    *accel_group);
void       gtk_window_remove_accel_group       (GtkWindow           *window,
						GtkAccelGroup	    *accel_group);
void       gtk_window_set_position             (GtkWindow           *window,
						GtkWindowPosition    position);
gboolean   gtk_window_activate_focus	       (GtkWindow           *window);
gboolean   gtk_window_activate_default	       (GtkWindow           *window);

void       gtk_window_set_transient_for        (GtkWindow           *window, 
						GtkWindow           *parent);
GtkWindow *gtk_window_get_transient_for        (GtkWindow           *window);
void       gtk_window_set_type_hint            (GtkWindow           *window, 
						GdkWindowTypeHint    hint);
GdkWindowTypeHint gtk_window_get_type_hint     (GtkWindow           *window);
void       gtk_window_set_destroy_with_parent  (GtkWindow           *window,
                                                gboolean             setting);
gboolean   gtk_window_get_destroy_with_parent  (GtkWindow           *window);

void       gtk_window_set_resizable            (GtkWindow           *window,
                                                gboolean             resizable);
gboolean   gtk_window_get_resizable            (GtkWindow           *window);

void       gtk_window_set_gravity              (GtkWindow           *window,
                                                GdkGravity           gravity);
GdkGravity gtk_window_get_gravity              (GtkWindow           *window);


void       gtk_window_set_geometry_hints       (GtkWindow           *window,
						GtkWidget           *geometry_widget,
						GdkGeometry         *geometry,
						GdkWindowHints       geom_mask);

/* gtk_window_set_has_frame () must be called before realizing the window_*/
void       gtk_window_set_has_frame            (GtkWindow *window, 
						gboolean   setting);
gboolean   gtk_window_get_has_frame            (GtkWindow *window);
void       gtk_window_set_frame_dimensions     (GtkWindow *window, 
						gint       left,
						gint       top,
						gint       right,
						gint       bottom);
void       gtk_window_get_frame_dimensions     (GtkWindow *window, 
						gint      *left,
						gint      *top,
						gint      *right,
						gint      *bottom);
void       gtk_window_set_decorated            (GtkWindow *window,
                                                gboolean   setting);
gboolean   gtk_window_get_decorated            (GtkWindow *window);

void       gtk_window_set_icon_list                (GtkWindow  *window,
                                                    GList      *list);
GList*     gtk_window_get_icon_list                (GtkWindow  *window);
void       gtk_window_set_icon                     (GtkWindow  *window,
                                                    GdkPixbuf  *icon);
GdkPixbuf* gtk_window_get_icon                     (GtkWindow  *window);
void       gtk_window_set_default_icon_list        (GList      *list);
GList*     gtk_window_get_default_icon_list        (void);

/* If window is set modal, input will be grabbed when show and released when hide */
void       gtk_window_set_modal      (GtkWindow *window,
				      gboolean   modal);
gboolean   gtk_window_get_modal      (GtkWindow *window);
GList*     gtk_window_list_toplevels (void);

void     gtk_window_add_mnemonic          (GtkWindow       *window,
					   guint            keyval,
					   GtkWidget       *target);
void     gtk_window_remove_mnemonic       (GtkWindow       *window,
					   guint            keyval,
					   GtkWidget       *target);
gboolean gtk_window_mnemonic_activate     (GtkWindow       *window,
					   guint            keyval,
					   GdkModifierType  modifier);
void     gtk_window_set_mnemonic_modifier (GtkWindow       *window,
					   GdkModifierType  modifier);
GdkModifierType gtk_window_get_mnemonic_modifier (GtkWindow *window);

void     gtk_window_present       (GtkWindow *window);
void     gtk_window_iconify       (GtkWindow *window);
void     gtk_window_deiconify     (GtkWindow *window);
void     gtk_window_stick         (GtkWindow *window);
void     gtk_window_unstick       (GtkWindow *window);
void     gtk_window_maximize      (GtkWindow *window);
void     gtk_window_unmaximize    (GtkWindow *window);

void gtk_window_begin_resize_drag (GtkWindow     *window,
                                   GdkWindowEdge  edge,
                                   gint           button,
                                   gint           root_x,
                                   gint           root_y,
                                   guint32        timestamp);
void gtk_window_begin_move_drag   (GtkWindow     *window,
                                   gint           button,
                                   gint           root_x,
                                   gint           root_y,
                                   guint32        timestamp);

#ifndef GTK_DISABLE_DEPRECATED
void       gtk_window_set_policy               (GtkWindow           *window,
						gint                 allow_shrink,
						gint                 allow_grow,
						gint                 auto_shrink);
#define	gtk_window_position			gtk_window_set_position
#endif /* GTK_DISABLE_DEPRECATED */

/* Set initial default size of the window (does not constrain user
 * resize operations)
 */
void     gtk_window_set_default_size (GtkWindow   *window,
                                      gint         width,
                                      gint         height);
void     gtk_window_get_default_size (GtkWindow   *window,
                                      gint        *width,
                                      gint        *height);
void     gtk_window_resize           (GtkWindow   *window,
                                      gint         width,
                                      gint         height);
void     gtk_window_get_size         (GtkWindow   *window,
                                      gint        *width,
                                      gint        *height);
void     gtk_window_move             (GtkWindow   *window,
                                      gint         x,
                                      gint         y);
void     gtk_window_get_position     (GtkWindow   *window,
                                      gint        *x,
                                      gint        *y);
gboolean gtk_window_parse_geometry   (GtkWindow   *window,
                                      const gchar *geometry);

/* Ignore this unless you are writing a GUI builder */
void     gtk_window_reshow_with_initial_size (GtkWindow *window);

/* Window groups
 */
GType            gtk_window_group_get_type      (void) G_GNUC_CONST;;

GtkWindowGroup * gtk_window_group_new           (void);
void             gtk_window_group_add_window    (GtkWindowGroup     *window_group,
						 GtkWindow          *window);
void             gtk_window_group_remove_window (GtkWindowGroup     *window_group,
					         GtkWindow          *window);

/* --- internal functions --- */
void            gtk_window_set_focus           (GtkWindow *window,
						GtkWidget *focus);
void            gtk_window_set_default         (GtkWindow *window,
						GtkWidget *defaultw);
void            gtk_window_remove_embedded_xid (GtkWindow *window,
						guint      xid);
void            gtk_window_add_embedded_xid    (GtkWindow *window,
						guint      xid);
void            _gtk_window_reposition         (GtkWindow *window,
						gint       x,
						gint       y);
void            _gtk_window_constrain_size     (GtkWindow *window,
						gint       width,
						gint       height,
						gint      *new_width,
						gint      *new_height);
GtkWindowGroup *_gtk_window_get_group          (GtkWindow *window);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_WINDOW_H__ */
