#include <config.h>
#include <gtk/gtkicontheme.h>
#include <stdlib.h>
#include <string.h>

static void
usage (void)
{
  g_print ("usage: test-icon-theme lookup <theme name> <icon name> [size]]\n"
	   " or\n"
	   "usage: test-icon-theme list <theme name> [context]\n");
}


int
main (int argc, char *argv[])
{
  GtkIconTheme *icon_theme;
  GtkIconInfo *icon_info;
  GdkRectangle embedded_rect;
  GdkPoint *attach_points;
  int n_attach_points;
  const gchar *display_name;
  char *context;
  char *themename;
  GList *list;
  int size = 48;
  int i;
  
  g_type_init ();

  if (argc < 3)
    {
      usage ();
      return 1;
    }

  themename = argv[2];
  
  icon_theme = gtk_icon_theme_new ();
  
  gtk_icon_theme_set_custom_theme (icon_theme, themename);

  if (strcmp (argv[1], "list") == 0)
    {
      if (argc >= 4)
	context = argv[3];
      else
	context = NULL;

      list = gtk_icon_theme_list_icons (icon_theme,
					   context);
      
      while (list)
	{
	  g_print ("%s\n", (char *)list->data);
	  list = list->next;
	}
    }
  else if (strcmp (argv[1], "lookup") == 0)
    {
      if (argc < 4)
	{
	  g_object_unref (icon_theme);
	  usage ();
	  return 1;
	}
      
      if (argc >= 5)
	size = atoi (argv[4]);
      
      icon_info = gtk_icon_theme_lookup_icon (icon_theme, argv[3], size, 0);
      g_print ("icon for %s at %dx%d is %s\n", argv[3], size, size,
	       icon_info ? gtk_icon_info_get_filename (icon_info) : "<none>");

      if (gtk_icon_info_get_embedded_rect (icon_info, &embedded_rect))
	{
	  g_print ("Embedded rect: %d,%d %dx%d\n",
		   embedded_rect.x, embedded_rect.y,
		   embedded_rect.width, embedded_rect.height);
	}

      if (gtk_icon_info_get_attach_points (icon_info, &attach_points, &n_attach_points))
	{
	  g_print ("Attach Points: ");
	  for (i = 0; i < n_attach_points; i++)
	    g_print ("%d, %d; ",
		     attach_points[i].x,
		     attach_points[i].y);
	  g_free (attach_points);
	}

      display_name = gtk_icon_info_get_display_name (icon_info);

      if (display_name)
	g_print ("Display name: %s\n", display_name);
      
      gtk_icon_info_free (icon_info);
    }
  else
    {
      g_object_unref (icon_theme);
      usage ();
      return 1;
    }
 

  g_object_unref (icon_theme);
  
  return 0;
}
