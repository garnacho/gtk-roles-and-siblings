/* Images
 *
 * GtkImage is used to display an image; the image can be in a number of formats.
 * Typically, you load an image into a GdkPixbuf, then display the pixbuf.
 *
 * This demo code shows some of the more obscure cases, in the simple
 * case a call to gtk_image_new_from_file() is all you need.
 *
 * If you want to put image data in your program as a C variable,
 * use the make-inline-pixbuf program that comes with GTK+.
 * This way you won't need to depend on loading external files, your
 * application binary can be self-contained.
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <errno.h>

static GtkWidget *window = NULL;
static GdkPixbufLoader *pixbuf_loader = NULL;
static guint load_timeout = 0;
static FILE* image_stream = NULL;

static void
progressive_prepared_callback (GdkPixbufLoader* loader, gpointer data)
{
  GdkPixbuf* pixbuf;
  GtkWidget* image;

  image = GTK_WIDGET (data);
  
  pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

  /* Avoid displaying random memory contents, since the pixbuf
   * isn't filled in yet.
   */
  gdk_pixbuf_fill (pixbuf, 0xaaaaaaff);
  
  gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
}

static void
progressive_updated_callback (GdkPixbufLoader* loader,
                              gint x, gint y, gint width, gint height,
                              gpointer data)
{
  GtkWidget* image;
  
  image = GTK_WIDGET (data);

  /* We know the pixbuf inside the GtkImage has changed, but the image
   * itself doesn't know this; so queue a redraw.  If we wanted to be
   * really efficient, we could use a drawing area or something
   * instead of a GtkImage, so we could control the exact position of
   * the pixbuf on the display, then we could queue a draw for only
   * the updated area of the image.
   */
  
  gtk_widget_queue_draw (image);
}

static gint
progressive_timeout (gpointer data)
{
  GtkWidget *image;

  image = GTK_WIDGET (data);
  
  /* This shows off fully-paranoid error handling, so looks scary.
   * You could factor out the error handling code into a nice separate
   * function to make things nicer.
   */
  
  if (image_stream)
    {
      size_t bytes_read;
      guchar buf[256];
      GError *error = NULL;
      
      bytes_read = fread (buf, 1, 256, image_stream);

      if (ferror (image_stream))
        {
          GtkWidget *dialog;
          
          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Failure reading image file 'alphatest.png': %s",
                                           g_strerror (errno));

          gtk_signal_connect (GTK_OBJECT (dialog),
                              "response",
                              GTK_SIGNAL_FUNC (gtk_widget_destroy),
                              NULL);

          fclose (image_stream);
          image_stream = NULL;

          gtk_widget_show (dialog);
          
          load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (!gdk_pixbuf_loader_write (pixbuf_loader,
                                    buf, bytes_read,
                                    &error))
        {
          GtkWidget *dialog;
          
          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Failed to load image: %s",
                                           error->message);

          g_error_free (error);
          
          gtk_signal_connect (GTK_OBJECT (dialog),
                              "response",
                              GTK_SIGNAL_FUNC (gtk_widget_destroy),
                              NULL);

          fclose (image_stream);
          image_stream = NULL;
          
          gtk_widget_show (dialog);

          load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (feof (image_stream))
        {
          fclose (image_stream);
          image_stream = NULL;

          /* Errors can happen on close, e.g. if the image
           * file was truncated we'll know on close that
           * it was incomplete.
           */
          error = NULL;
          if (!gdk_pixbuf_loader_close (pixbuf_loader,
                                        &error))
            {
              GtkWidget *dialog;
              
              dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "Failed to load image: %s",
                                               error->message);
              
              g_error_free (error);
              
              gtk_signal_connect (GTK_OBJECT (dialog),
                                  "response",
                                  GTK_SIGNAL_FUNC (gtk_widget_destroy),
                                  NULL);
              
              gtk_widget_show (dialog);

              g_object_unref (G_OBJECT (pixbuf_loader));
              pixbuf_loader = NULL;
              
              load_timeout = 0;
              
              return FALSE; /* uninstall the timeout */
            }
          
          g_object_unref (G_OBJECT (pixbuf_loader));
          pixbuf_loader = NULL;
        }
    }
  else
    {
      const gchar *filename;

      if (g_file_test ("./alphatest.png", G_FILE_TEST_EXISTS))
        filename = "./alphatest.png";
      else
        filename = DEMOCODEDIR"/alphatest.png";
      
      image_stream = fopen (filename, "r");

      if (image_stream == NULL)
        {
          GtkWidget *dialog;
          
          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_DESTROY_WITH_PARENT,
                                           GTK_MESSAGE_ERROR,
                                           GTK_BUTTONS_CLOSE,
                                           "Unable to open image file 'alphatest.png': %s",
                                           g_strerror (errno));

          gtk_signal_connect (GTK_OBJECT (dialog),
                              "response",
                              GTK_SIGNAL_FUNC (gtk_widget_destroy),
                              NULL);
          
          gtk_widget_show (dialog);

          load_timeout = 0;

          return FALSE; /* uninstall the timeout */
        }

      if (pixbuf_loader)
        {
          gdk_pixbuf_loader_close (pixbuf_loader, NULL);
          g_object_unref (G_OBJECT (pixbuf_loader));
          pixbuf_loader = NULL;
        }
      
      pixbuf_loader = gdk_pixbuf_loader_new ();
      
      g_signal_connect_data (G_OBJECT (pixbuf_loader),
                             "area_prepared",
                             G_CALLBACK (progressive_prepared_callback),
                             image,
                             NULL, FALSE, FALSE);
      
      g_signal_connect_data (G_OBJECT (pixbuf_loader),
                             "area_updated",
                             G_CALLBACK (progressive_updated_callback),
                             image,
                             NULL, FALSE, FALSE);
    }

  /* leave timeout installed */
  return TRUE;
}

static void
start_progressive_loading (GtkWidget *image)
{
  /* This is obviously totally contrived (we slow down loading
   * on purpose to show how incremental loading works).
   * The real purpose of incremental loading is the case where
   * you are reading data from a slow source such as the network.
   * The timeout simply simulates a slow data source by inserting
   * pauses in the reading process.
   */
  load_timeout = g_timeout_add (150,
                                progressive_timeout,
                                image);
}

static void
cleanup_callback (GtkObject *object,
                  gpointer data)
{
  if (load_timeout)
    {
      g_source_remove (load_timeout);
      load_timeout = 0;
    }
  
  if (pixbuf_loader)
    {
      gdk_pixbuf_loader_close (pixbuf_loader, NULL);
      g_object_unref (G_OBJECT (pixbuf_loader));
      pixbuf_loader = NULL;
    }

  if (image_stream)
    fclose (image_stream);
  image_stream = NULL;
}

GtkWidget *
do_images (void)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *align;
  
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Images");

      gtk_signal_connect (GTK_OBJECT (window), "destroy", GTK_SIGNAL_FUNC (gtk_widget_destroyed), &window);
      gtk_signal_connect (GTK_OBJECT (window), "destroy", GTK_SIGNAL_FUNC (cleanup_callback), NULL);

      gtk_container_set_border_width (GTK_CONTAINER (window), 8);

      vbox = gtk_vbox_new (FALSE, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "<u>Image loaded from a file</u>");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      
      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      /* The alignment keeps the frame from growing when users resize
       * the window
       */
      align = gtk_alignment_new (0.5, 0.5, 0, 0);
      gtk_container_add (GTK_CONTAINER (align), frame);
      gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

      /* We look for the image in the current directory first,
       * so you can run gtk-demo without installing GTK
       */
      if (g_file_test ("./gtk-logo-rgb.gif", G_FILE_TEST_EXISTS))
        {
          /* This code shows off error handling. You can just use
           * gtk_image_new_from_file() instead if you don't want to report
           * errors to the user. If the file doesn't load when using
           * gtk_image_new_from_file(), a "missing image" icon will
           * be displayed instead.
           */
          GdkPixbuf *pixbuf;
          GError *error = NULL;
          
          pixbuf = gdk_pixbuf_new_from_file ("./gtk-logo-rgb.gif",
                                             &error);
          if (error)
            {
              GtkWidget *dialog;

              dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                               GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_CLOSE,
                                               "Unable to open image file 'gtk-logo-rgb.gif': %s",
                                               error->message);
              g_error_free (error);
              
              gtk_signal_connect (GTK_OBJECT (dialog),
                                  "response",
                                  GTK_SIGNAL_FUNC (gtk_widget_destroy),
                                  NULL);
              
              gtk_widget_show (dialog);
            }
          
          image = gtk_image_new_from_pixbuf (pixbuf);
        }
      else
        {
          /* This is the simpler code, with no error handling.
           * Here we're loading the installed gtk-logo-rgb.gif instead
           * of the one in the current directory.
           */
          image = gtk_image_new_from_file (DEMOCODEDIR"/gtk-logo-rgb.gif");
        }

      gtk_container_add (GTK_CONTAINER (frame), image);


      /* Animation */

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "<u>Animation loaded from a file</u>");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      
      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      /* The alignment keeps the frame from growing when users resize
       * the window
       */
      align = gtk_alignment_new (0.5, 0.5, 0, 0);
      gtk_container_add (GTK_CONTAINER (align), frame);
      gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

      /* We look for the image in the current directory first,
       * so you can run gtk-demo without installing GTK
       */
      if (g_file_test ("./floppybuddy.gif", G_FILE_TEST_EXISTS))
        image = gtk_image_new_from_file ("./floppybuddy.gif");
      else
        image = gtk_image_new_from_file (DEMOCODEDIR"/floppybuddy.gif");

      gtk_container_add (GTK_CONTAINER (frame), image);
      

      /* Progressive */
      
      
      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "<u>Progressive image loading</u>");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
      
      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      /* The alignment keeps the frame from growing when users resize
       * the window
       */
      align = gtk_alignment_new (0.5, 0.5, 0, 0);
      gtk_container_add (GTK_CONTAINER (align), frame);
      gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

      /* Create an empty image for now; the progressive loader
       * will create the pixbuf and fill it in.
       */
      image = gtk_image_new_from_pixbuf (NULL);
      gtk_container_add (GTK_CONTAINER (frame), image);

      start_progressive_loading (image);
    }

  if (!GTK_WIDGET_VISIBLE (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
