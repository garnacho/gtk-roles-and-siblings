#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTES_PER_OUTPUT_LINE 15

static gint
output_byte (guchar byte,
	     gint   online)
{
  if (online == BYTES_PER_OUTPUT_LINE)
    {
      printf (",\n  ");
      online = 0;
    }
  else if (online)
    {
      printf (",");
    }

  printf ("0x%02x", byte);
  return online + 1;
}

static void
do_part (GdkPixbuf  *pixbuf,
	 gint        part1_index,
	 gint        part2_index,
	 gint        part3_index,
	 const char *base_name,
	 const char *part_name)
{
  const guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
  const guchar *color1;
  const guchar *color2;
  const guchar *color3;
  gint rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  gint n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  gint width = gdk_pixbuf_get_width (pixbuf);
  gint height = gdk_pixbuf_get_height (pixbuf);
  gint online = 0;

  color1 = pixels + part1_index * n_channels;
  color2 = pixels + part2_index * n_channels;
  color3 = pixels + part3_index * n_channels;
  pixels += rowstride;
  
  printf ("static guchar %s_%s_bits[] = {\n", base_name, part_name);
  printf ("  ");

  while (height--)
    {
      guchar bit = 1;
      guchar byte = 0;
      const guchar *p = pixels;
      gint n = width;

      while (n--)
	{
	  if ((part1_index >= 0 && memcmp (p, color1, n_channels) == 0) ||
	      (part2_index >= 0 && memcmp (p, color2, n_channels) == 0) ||
	      (part3_index >= 0 && memcmp (p, color3, n_channels) == 0))
	    byte |= bit;

	  if (bit == 0x80)
	    {
	      online = output_byte (byte, online);
	      byte = 0;
	      bit = 1;
	    }
	  else
	    bit <<= 1;

	  p += n_channels;
	}

      if (width & 7)		/* a leftover partial byte */
	online = output_byte (byte, online);

      pixels += rowstride;
    }
  
  printf ("};\n");
}

typedef enum {
  PART_BLACK,
  PART_DARK,
  PART_MID,
  PART_LIGHT,
  PART_TEXT,
  PART_TEXT_AA,
  PART_BASE,
  PART_LAST
} Part;

const char *part_names[PART_LAST] = {
  "black",
  "dark",
  "mid",
  "light",
  "text",
  "aa",
  "base",
};

int main (int argc, char **argv)
{
  gchar *progname = g_path_get_basename (argv[0]);
  GdkPixbuf *pixbuf;
  GError *error = NULL;
  gint i;

  if (argc != 3)
    {
      fprintf (stderr, "%s: Usage: %s FILE BASE\n", progname, progname);
      exit (1);
    }

  g_type_init ();
  
  pixbuf = gdk_pixbuf_new_from_file (argv[1], &error);
  if (!pixbuf)
    {
      fprintf (stderr, "%s: cannot open file '%s': %s\n", progname, argv[1], error->message);
      exit (1);
    }
  
  if (gdk_pixbuf_get_width (pixbuf) < PART_LAST)
    {
      fprintf (stderr, "%s: source image must be at least %d pixels wide\n", progname, PART_LAST);
      exit (1);
    }

  if (gdk_pixbuf_get_height (pixbuf) < 1)
    {
      fprintf (stderr, "%s: source image must be at least 1 pixel height\n", progname);
      exit (1);
    }

  printf ("/*\n * Extracted from %s, width=%d, height=%d\n */\n", argv[1],
	  gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf) - 1);

  for (i = 0; i < PART_LAST; i++)
    {
      /* As a bit of a hack, we want the base image to extend over the text
       * and text_aa parts so that we can draw the image either with or without
       * the indicator
       */
      if (i == PART_BASE)
	do_part (pixbuf, PART_BASE, PART_TEXT_AA, PART_TEXT, argv[2], part_names[i]);
      else
	do_part (pixbuf, i, -1, -1, argv[2], part_names[i]);
    }
  return 0;
}
