/* GTK - The GIMP Toolkit
 * Copyright (C) 2000 Red Hat, Inc.
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
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include "gtkaccelgroup.h"
#include "gtkimcontextsimple.h"
#include "gtksettings.h"
#include "gtkwidget.h"
#include "gtkintl.h"
#include "gtkalias.h"

typedef struct _GtkComposeTable GtkComposeTable;
typedef struct _GtkComposeTableCompact GtkComposeTableCompact;

struct _GtkComposeTable 
{
  const guint16 *data;
  gint max_seq_len;
  gint n_seqs;
};

struct _GtkComposeTableCompact
{
  const guint16 *data;
  gint max_seq_len;
  gint n_index_size;
  gint n_index_stride;
};

/* This file contains the table of the compose sequences, 
 * static const guint16 gtk_compose_seqs_compact[] = {}
 * IT is generated from the compose-parse.py script.
 */
#include "gtkimcontextsimpleseqs.h"

/* From the values below, the value 22 means the number of different first keysyms 
 * that exist in the Compose file (from Xorg). When running compose-parse.py without 
 * parameters, you get the count that you can put here. Needed when updating the
 * gtkimcontextsimpleseqs.h header file (contains the compose sequences).
 */
static const GtkComposeTableCompact gtk_compose_table_compact = {
  gtk_compose_seqs_compact,
  5,
  21,
  6
};

static const guint16 gtk_compose_ignore[] = {
  GDK_Shift_L,
  GDK_Shift_R,
  GDK_Control_L,
  GDK_Control_R,
  GDK_Caps_Lock,
  GDK_Shift_Lock,
  GDK_Meta_L,
  GDK_Meta_R,
  GDK_Alt_L,
  GDK_Alt_R,
  GDK_Super_L,
  GDK_Super_R,
  GDK_Hyper_L,
  GDK_Hyper_R,
  GDK_Mode_switch,
  GDK_ISO_Level3_Shift
};

static void     gtk_im_context_simple_finalize           (GObject                  *obj);
static gboolean gtk_im_context_simple_filter_keypress    (GtkIMContext             *context,
							  GdkEventKey              *key);
static void     gtk_im_context_simple_reset              (GtkIMContext             *context);
static void     gtk_im_context_simple_get_preedit_string (GtkIMContext             *context,
							  gchar                   **str,
							  PangoAttrList           **attrs,
							  gint                     *cursor_pos);

G_DEFINE_TYPE (GtkIMContextSimple, gtk_im_context_simple, GTK_TYPE_IM_CONTEXT)

static void
gtk_im_context_simple_class_init (GtkIMContextSimpleClass *class)
{
  GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS (class);
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  im_context_class->filter_keypress = gtk_im_context_simple_filter_keypress;
  im_context_class->reset = gtk_im_context_simple_reset;
  im_context_class->get_preedit_string = gtk_im_context_simple_get_preedit_string;
  gobject_class->finalize = gtk_im_context_simple_finalize;
}

static void
gtk_im_context_simple_init (GtkIMContextSimple *im_context_simple)
{  
}

static void
gtk_im_context_simple_finalize (GObject *obj)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (obj);

  if (context_simple->tables)
    {
      g_slist_foreach (context_simple->tables, (GFunc)g_free, NULL);
      g_slist_free (context_simple->tables);

      context_simple->tables = NULL;
    }

  G_OBJECT_CLASS (gtk_im_context_simple_parent_class)->finalize (obj);
}

/** 
 * gtk_im_context_simple_new:
 * 
 * Creates a new #GtkIMContextSimple.
 *
 * Returns: a new #GtkIMContextSimple.
 **/
GtkIMContext *
gtk_im_context_simple_new (void)
{
  return g_object_new (GTK_TYPE_IM_CONTEXT_SIMPLE, NULL);
}

static void
gtk_im_context_simple_commit_char (GtkIMContext *context,
				   gunichar ch)
{
  gchar buf[10];
  gint len;

  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);

  g_return_if_fail (g_unichar_validate (ch));
  
  len = g_unichar_to_utf8 (ch, buf);
  buf[len] = '\0';

  if (context_simple->tentative_match || context_simple->in_hex_sequence)
    {
      context_simple->in_hex_sequence = FALSE;  
      context_simple->tentative_match = 0;
      context_simple->tentative_match_len = 0;
      g_signal_emit_by_name (context_simple, "preedit_changed");
      g_signal_emit_by_name (context_simple, "preedit_end");
    }

  g_signal_emit_by_name (context, "commit", &buf);
}

static int
compare_seq_index (const void *key, const void *value)
{
  const guint *keysyms = key;
  const guint16 *seq = value;

  if (keysyms[0] < seq[0])
    return -1;
  else if (keysyms[0] > seq[0])
    return 1;

  return 0;
}

static int
compare_seq (const void *key, const void *value)
{
  int i = 0;
  const guint *keysyms = key;
  const guint16 *seq = value;

  while (keysyms[i])
    {
      if (keysyms[i] < seq[i])
	return -1;
      else if (keysyms[i] > seq[i])
	return 1;

      i++;
    }

  return 0;
}

static gboolean
check_table (GtkIMContextSimple    *context_simple,
	     const GtkComposeTable *table,
	     gint                   n_compose)
{
  gint row_stride = table->max_seq_len + 2; 
  guint16 *seq; 
  
  /* Will never match, if the sequence in the compose buffer is longer
   * than the sequences in the table.  Further, compare_seq (key, val)
   * will overrun val if key is longer than val. */
  if (n_compose > table->max_seq_len)
    return FALSE;
  
  seq = bsearch (context_simple->compose_buffer,
		 table->data, table->n_seqs,
		 sizeof (guint16) *  row_stride, 
		 compare_seq);

  if (seq)
    {
      guint16 *prev_seq;

      /* Back up to the first sequence that matches to make sure
       * we find the exact match if their is one.
       */
      while (seq > table->data)
	{
	  prev_seq = seq - row_stride;
	  if (compare_seq (context_simple->compose_buffer, prev_seq) != 0)
	    break;
	  seq = prev_seq;
	}
      
      if (n_compose == table->max_seq_len ||
	  seq[n_compose] == 0) /* complete sequence */
	{
	  guint16 *next_seq;
	  gunichar value = 
	    0x10000 * seq[table->max_seq_len] + seq[table->max_seq_len + 1];

	  
	  /* We found a tentative match. See if there are any longer
	   * sequences containing this subsequence
	   */
	  next_seq = seq + row_stride;
	  if (next_seq < table->data + row_stride * table->n_seqs)
	    {
	      if (compare_seq (context_simple->compose_buffer, next_seq) == 0)
		{
		  context_simple->tentative_match = value;
		  context_simple->tentative_match_len = n_compose;
		
		  g_signal_emit_by_name (context_simple, "preedit_changed");

		  return TRUE;
		}
	    }

	  gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple), value);
	  context_simple->compose_buffer[0] = 0;
	}
      
      return TRUE;
    }

  return FALSE;
}

static gboolean
check_compact_table (GtkIMContextSimple    *context_simple,
	     const GtkComposeTableCompact *table,
	     gint                   n_compose)
{
  gint row_stride;
  guint16 *seq_index;
  guint16 *seq; 
  gint i;

  /* Will never match, if the sequence in the compose buffer is longer
   * than the sequences in the table.  Further, compare_seq (key, val)
   * will overrun val if key is longer than val. */
  if (n_compose > table->max_seq_len)
    return FALSE;
  
  seq_index = bsearch (context_simple->compose_buffer,
		 table->data, table->n_index_size,
		 sizeof (guint16) *  table->n_index_stride, 
		 compare_seq_index);

  if (!seq_index)
    return FALSE;

  if (seq_index && n_compose == 1)
    return TRUE;

  seq = NULL;

  for (i = n_compose-1; i < table->max_seq_len; i++)
    {
      row_stride = i + 1;

      if (seq_index[i+1] - seq_index[i] > 0)
        {
	  seq = bsearch (context_simple->compose_buffer + 1,
		 table->data + seq_index[i], (seq_index[i+1] - seq_index[i]) / row_stride,
		 sizeof (guint16) *  row_stride, 
		 compare_seq);

	  if (seq)
            {
              if (i == n_compose - 1)
                break;
              else
                {
                  g_signal_emit_by_name (context_simple, "preedit_changed");
      		  return TRUE;
                }
             }
        }
    }

  if (!seq)
    return FALSE;
  else
    {
      gunichar value;

      value = seq[row_stride - 1];
	  
      gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple), value);
      context_simple->compose_buffer[0] = 0;

      return TRUE;
    }

  return FALSE;
}

/* When updating the table of the compose sequences, also update here.
 */
#define IS_DEAD_KEY(k) \
    (((k) >= GDK_dead_grave && (k) <= GDK_dead_stroke) || \
     g_unichar_type (gdk_keyval_to_unicode (k)) == G_UNICODE_NON_SPACING_MARK)

static gboolean
check_algorithmically (GtkIMContextSimple    *context_simple,
		       gint                   n_compose)

{
  int i;
  int k;
  gunichar combination_buffer[GTK_MAX_COMPOSE_LEN];
  gunichar combination_buffer_temp[GTK_MAX_COMPOSE_LEN];
  gchar *combination_utf8, *nfc;
  gchar *combination_utf8_temp = NULL, *nfc_temp = NULL;

  if (n_compose >= GTK_MAX_COMPOSE_LEN)
    return FALSE;

  for (i = 0; i < n_compose && IS_DEAD_KEY (context_simple->compose_buffer[i]); i++)
    ;
  if (i == n_compose)
    return TRUE;

  if (i > 0 && i == n_compose - 1)
    {
      combination_buffer[0] = gdk_keyval_to_unicode (context_simple->compose_buffer[i]);
      combination_buffer[n_compose] = 0;
      i--;
      while (i >= 0)
	{
	  switch (context_simple->compose_buffer[i])
	    {
#define CASE(keysym, unicode) \
	    case GDK_dead_##keysym: combination_buffer[i+1] = unicode; break

	    CASE (grave, 0x0300);
	    CASE (acute, 0x0301);
	    CASE (circumflex, 0x0302);
	    CASE (tilde, 0x0303);	/* Normally is 0x303; Greek Polytonic needs 0x342.
					 * We modified the compose sequences for now
					 * so that for Greek we don't apply algorithmic
					 * when perispomeni (0x342) is required
					 * Filed report; pending, bug 14013 (Freedesktop).
					 */
	    CASE (macron, 0x0304);
	    CASE (breve, 0x0306);
	    CASE (abovedot, 0x0307);
	    CASE (diaeresis, 0x0308);
	    CASE (abovering, 0x030A);
	    CASE (doubleacute, 0x030B);
	    CASE (caron, 0x030C);
	    CASE (cedilla, 0x0327);
	    CASE (ogonek, 0x0328);	/* Normally is 0x328; Greek Polytonic needs 0x314.
					 * We modified the compose sequences for now to 
					 * so that for Greek we don't apply algorithmic
					 * when dasia (0x314) is required
					 * Patch accepted in Xorg/GIT, may take a bit to propagate.
					 */
	    CASE (dasia, 0x314);
	    CASE (iota, 0x0345); /* Used by Greek Polytonic layout only; "ypogegrammeni" */
	    CASE (voiced_sound, 0x3099);	/* Per Marcus Khun keysyms.txt file. */
	    CASE (semivoiced_sound, 0x309a);	/* Per Marcus Khun keysyms.txt file. */
	    CASE (belowdot, 0x0323);
	    CASE (hook, 0x0309);
	    CASE (horn, 0x031b);	/* Normally is 0x31b; Greek Polytonic needs 0x313 (or 0x343).
					 * We modified the compose sequences for now to 
					 * so that for Greek we don't apply algorithmic
					 * when psili (0x343) is required
					 * Patch accepted in Xorg/GIT, may take a bit to propagate.
					 */
	    CASE (psili, 0x343);
#undef CASE
	    default:
	      combination_buffer[i+1] = gdk_keyval_to_unicode (context_simple->compose_buffer[i]);
	    }
	  i--;
	}
      
      if (n_compose > 2)
	{
	  gint n_combinations;
	  gunichar temp_swap;

	  /* We calculate the number of permutations of the diacritic marks, factorial(n_compose-1).
 	   * When diacritic marks belong to the same Canonical Combining Class, 
 	   * a normalisation does not attempt reorder them, thus we do this ourselves.
   	   */
	  n_combinations = 1;
	  for (k = 1; k < n_compose; k++ )
	     n_combinations *= k;

	  memcpy (combination_buffer_temp, combination_buffer, GTK_MAX_COMPOSE_LEN * sizeof (gunichar) );

	  for (k = 0; k < n_combinations; k++ )
	     {
	       g_unicode_canonical_ordering (combination_buffer_temp, n_compose);
      	       combination_utf8_temp = g_ucs4_to_utf8 (combination_buffer_temp, -1, NULL, NULL, NULL);
               nfc_temp = g_utf8_normalize (combination_utf8_temp, -1, G_NORMALIZE_NFC);	       	

	       if (g_utf8_strlen (nfc_temp, -1) == 1)
	         {
	  	   memcpy (combination_buffer, combination_buffer_temp, GTK_MAX_COMPOSE_LEN * sizeof (gunichar) );
		   break;
		 }

	       temp_swap = combination_buffer_temp[k % (n_compose - 1) + 1];
	       combination_buffer_temp[k % (n_compose - 1) + 1] = combination_buffer_temp[(k+1) % (n_compose - 1) + 1];
	       combination_buffer_temp[(k+1) % (n_compose - 1) + 1] = temp_swap;
	     }

	  g_free (combination_utf8_temp);
	  g_free (nfc_temp);
	}

      combination_utf8 = g_ucs4_to_utf8 (combination_buffer, -1, NULL, NULL, NULL);
      nfc = g_utf8_normalize (combination_utf8, -1, G_NORMALIZE_NFC);
      if (g_utf8_strlen (nfc, -1) == 1)
	{
	  gunichar value = g_utf8_get_char (nfc);
	  gtk_im_context_simple_commit_char (GTK_IM_CONTEXT (context_simple), value);
	  context_simple->compose_buffer[0] = 0;

	  g_free (combination_utf8);
	  g_free (nfc);
	  return TRUE;
	}
      g_free (combination_utf8);
      g_free (nfc);
    }

  return FALSE;
}

/* In addition to the table-driven sequences, we allow Unicode hex
 * codes to be entered. The method chosen here is similar to the
 * one recommended in ISO 14755, but not exactly the same, since we
 * don't want to steal 16 valuable key combinations. 
 * 
 * A hex Unicode sequence must be started with Ctrl-Shift-U, followed
 * by a sequence of hex digits entered with Ctrl-Shift still held.
 * Releasing one of the modifiers or pressing space while the modifiers
 * are still held commits the character. It is possible to erase
 * digits using backspace.
 *
 * As an extension to the above, we also allow to start the sequence
 * with Ctrl-Shift-U, then release the modifiers before typing any
 * digits, and enter the digits without modifiers.
 */
#define HEX_MOD_MASK (GDK_CONTROL_MASK | GDK_SHIFT_MASK)

static gboolean
check_hex (GtkIMContextSimple *context_simple,
           gint                n_compose)
{
  /* See if this is a hex sequence, return TRUE if so */
  gint i;
  GString *str;
  gulong n;
  gchar *nptr = NULL;
  gchar buf[7];

  context_simple->tentative_match = 0;
  context_simple->tentative_match_len = 0;

  str = g_string_new (NULL);
  
  i = 0;
  while (i < n_compose)
    {
      gunichar ch;
      
      ch = gdk_keyval_to_unicode (context_simple->compose_buffer[i]);
      
      if (ch == 0)
        return FALSE;

      if (!g_unichar_isxdigit (ch))
        return FALSE;

      buf[g_unichar_to_utf8 (ch, buf)] = '\0';

      g_string_append (str, buf);
      
      ++i;
    }

  n = strtoul (str->str, &nptr, 16);

  /* if strtoul fails it probably means non-latin digits were used;
   * we should in principle handle that, but we probably don't.
   */
  if (nptr - str->str < str->len)
    {
      g_string_free (str, TRUE);
      return FALSE;
    }
  else
    g_string_free (str, TRUE);

  if (g_unichar_validate (n))
    {
      context_simple->tentative_match = n;
      context_simple->tentative_match_len = n_compose;
    }
  
  return TRUE;
}

static void
beep_window (GdkWindow *window)
{
  GtkWidget *widget;

  gdk_window_get_user_data (window, (gpointer) &widget);

  if (GTK_IS_WIDGET (widget))
    {
      gtk_widget_error_bell (widget);
    }
  else
    {
      GdkScreen *screen = gdk_drawable_get_screen (GDK_DRAWABLE (window));
      gboolean   beep;

      g_object_get (gtk_settings_get_for_screen (screen),
                    "gtk-error-bell", &beep,
                    NULL);

      if (beep)
        gdk_window_beep (window);
    }
}

static gboolean
no_sequence_matches (GtkIMContextSimple *context_simple,
                     gint                n_compose,
                     GdkEventKey        *event)
{
  GtkIMContext *context;
  gunichar ch;
  
  context = GTK_IM_CONTEXT (context_simple);
  
  /* No compose sequences found, check first if we have a partial
   * match pending.
   */
  if (context_simple->tentative_match)
    {
      gint len = context_simple->tentative_match_len;
      int i;
      
      gtk_im_context_simple_commit_char (context, context_simple->tentative_match);
      context_simple->compose_buffer[0] = 0;
      
      for (i=0; i < n_compose - len - 1; i++)
	{
	  GdkEvent *tmp_event = gdk_event_copy ((GdkEvent *)event);
	  tmp_event->key.keyval = context_simple->compose_buffer[len + i];
	  
	  gtk_im_context_filter_keypress (context, (GdkEventKey *)tmp_event);
	  gdk_event_free (tmp_event);
	}

      return gtk_im_context_filter_keypress (context, event);
    }
  else
    {
      context_simple->compose_buffer[0] = 0;
      if (n_compose > 1)		/* Invalid sequence */
	{
	  beep_window (event->window);
	  return TRUE;
	}
  
      ch = gdk_keyval_to_unicode (event->keyval);
      if (ch != 0)
	{
	  gtk_im_context_simple_commit_char (context, ch);
	  return TRUE;
	}
      else
	return FALSE;
    }
}

static gboolean
is_hex_keyval (guint keyval)
{
  gunichar ch = gdk_keyval_to_unicode (keyval);

  return g_unichar_isxdigit (ch);
}

static guint
canonical_hex_keyval (GdkEventKey *event)
{
  GdkKeymap *keymap = gdk_keymap_get_for_display (gdk_drawable_get_display (event->window));
  guint keyval;
  guint *keyvals = NULL;
  gint n_vals = 0;
  gint i;
  
  /* See if the keyval is already a hex digit */
  if (is_hex_keyval (event->keyval))
    return event->keyval;

  /* See if this key would have generated a hex keyval in
   * any other state, and return that hex keyval if so
   */
  gdk_keymap_get_entries_for_keycode (keymap,
				      event->hardware_keycode,
				      NULL,
				      &keyvals, &n_vals);

  keyval = 0;
  i = 0;
  while (i < n_vals)
    {
      if (is_hex_keyval (keyvals[i]))
        {
          keyval = keyvals[i];
          break;
        }

      ++i;
    }

  g_free (keyvals);
  
  if (keyval)
    return keyval;
  else
    /* No way to make it a hex digit
     */
    return 0;
}

static gboolean
gtk_im_context_simple_filter_keypress (GtkIMContext *context,
				       GdkEventKey  *event)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);
  GSList *tmp_list;  
  int n_compose = 0;
  gboolean have_hex_mods;
  gboolean is_hex_start;
  gboolean is_hex_end;
  gboolean is_backspace;
  gboolean is_escape;
  guint hex_keyval;
  int i;

  while (context_simple->compose_buffer[n_compose] != 0)
    n_compose++;

  if (event->type == GDK_KEY_RELEASE)
    {
      if (context_simple->in_hex_sequence &&
	  (event->keyval == GDK_Control_L || event->keyval == GDK_Control_R ||
	   event->keyval == GDK_Shift_L || event->keyval == GDK_Shift_R))
	{
	  if (context_simple->tentative_match &&
	      g_unichar_validate (context_simple->tentative_match))
	    {
	      gtk_im_context_simple_commit_char (context, context_simple->tentative_match);
	      context_simple->compose_buffer[0] = 0;

	    }
	  else if (n_compose == 0)
	    {
	      context_simple->modifiers_dropped = TRUE;
	    }
	  else
	    {
	      /* invalid hex sequence */
	      beep_window (event->window);
	      
	      context_simple->tentative_match = 0;
	      context_simple->in_hex_sequence = FALSE;
	      context_simple->compose_buffer[0] = 0;
	      
	      g_signal_emit_by_name (context_simple, "preedit_changed");
	      g_signal_emit_by_name (context_simple, "preedit_end");
	    }

	  return TRUE;
	}
      else
	return FALSE;
    }

  /* Ignore modifier key presses */
  for (i = 0; i < G_N_ELEMENTS (gtk_compose_ignore); i++)
    if (event->keyval == gtk_compose_ignore[i])
      return FALSE;

  if (context_simple->in_hex_sequence && context_simple->modifiers_dropped)
    have_hex_mods = TRUE;
  else
    have_hex_mods = (event->state & (HEX_MOD_MASK)) == HEX_MOD_MASK;
  is_hex_start = event->keyval == GDK_U;
  is_hex_end = (event->keyval == GDK_space || 
		event->keyval == GDK_KP_Space ||
		event->keyval == GDK_Return || 
		event->keyval == GDK_ISO_Enter ||
		event->keyval == GDK_KP_Enter);
  is_backspace = event->keyval == GDK_BackSpace;
  is_escape = event->keyval == GDK_Escape;
  hex_keyval = canonical_hex_keyval (event);

  /* If we are already in a non-hex sequence, or
   * this keystroke is not hex modifiers + hex digit, don't filter 
   * key events with accelerator modifiers held down.
   */
  if (!have_hex_mods ||
      (n_compose > 0 && !context_simple->in_hex_sequence) || 
      (n_compose == 0 && !context_simple->in_hex_sequence && !is_hex_start) ||
      (context_simple->in_hex_sequence && !hex_keyval && 
       !is_hex_start && !is_hex_end && !is_escape && !is_backspace))
    {
      if (event->state & (gtk_accelerator_get_default_mod_mask () & ~GDK_SHIFT_MASK) ||
	  (context_simple->in_hex_sequence && context_simple->modifiers_dropped &&
	   (event->keyval == GDK_Return || 
	    event->keyval == GDK_ISO_Enter ||
	    event->keyval == GDK_KP_Enter)))
	{
	  return FALSE;
	}
    }
  
  /* Handle backspace */
  if (context_simple->in_hex_sequence && have_hex_mods && is_backspace)
    {
      if (n_compose > 0)
	{
	  n_compose--;
	  context_simple->compose_buffer[n_compose] = 0;
          check_hex (context_simple, n_compose);
	}
      else
	{
	  context_simple->in_hex_sequence = FALSE;
	}

      g_signal_emit_by_name (context_simple, "preedit_changed");

      if (!context_simple->in_hex_sequence)
        g_signal_emit_by_name (context_simple, "preedit_end");
      
      return TRUE;
    }

  /* Check for hex sequence restart */
  if (context_simple->in_hex_sequence && have_hex_mods && is_hex_start)
    {
      if (context_simple->tentative_match &&
	  g_unichar_validate (context_simple->tentative_match))
	{
	  gtk_im_context_simple_commit_char (context, context_simple->tentative_match);
	  context_simple->compose_buffer[0] = 0;
	}
      else 
	{
	  /* invalid hex sequence */
	  if (n_compose > 0)
	    beep_window (event->window);
	  
	  context_simple->tentative_match = 0;
	  context_simple->in_hex_sequence = FALSE;
	  context_simple->compose_buffer[0] = 0;
	}
    }
  
  /* Check for hex sequence start */
  if (!context_simple->in_hex_sequence && have_hex_mods && is_hex_start)
    {
      context_simple->compose_buffer[0] = 0;
      context_simple->in_hex_sequence = TRUE;
      context_simple->modifiers_dropped = FALSE;
      context_simple->tentative_match = 0;

      g_signal_emit_by_name (context_simple, "preedit_start");
      g_signal_emit_by_name (context_simple, "preedit_changed");
  
      return TRUE;
    }
  
  /* Then, check for compose sequences */
  if (context_simple->in_hex_sequence)
    {
      if (hex_keyval)
	context_simple->compose_buffer[n_compose++] = hex_keyval;
      else if (is_escape)
	{
	  gtk_im_context_simple_reset (context);
	  
	  return TRUE;
	}
      else if (!is_hex_end)
	{
	  /* non-hex character in hex sequence */
	  beep_window (event->window);
	  
	  return TRUE;
	}
    }
  else
    context_simple->compose_buffer[n_compose++] = event->keyval;

  context_simple->compose_buffer[n_compose] = 0;

  if (context_simple->in_hex_sequence)
    {
      /* If the modifiers are still held down, consider the sequence again */
      if (have_hex_mods)
        {
          /* space or return ends the sequence, and we eat the key */
          if (n_compose > 0 && is_hex_end)
            {
	      if (context_simple->tentative_match &&
		  g_unichar_validate (context_simple->tentative_match))
		{
		  gtk_im_context_simple_commit_char (context, context_simple->tentative_match);
		  context_simple->compose_buffer[0] = 0;
		}
	      else
		{
		  /* invalid hex sequence */
		  beep_window (event->window);

		  context_simple->tentative_match = 0;
		  context_simple->in_hex_sequence = FALSE;
		  context_simple->compose_buffer[0] = 0;
		}
            }
          else if (!check_hex (context_simple, n_compose))
	    beep_window (event->window);
	  
	  g_signal_emit_by_name (context_simple, "preedit_changed");

	  if (!context_simple->in_hex_sequence)
	    g_signal_emit_by_name (context_simple, "preedit_end");

	  return TRUE;
        }
    }
  else
    {
      tmp_list = context_simple->tables;
      while (tmp_list)
        {
          if (check_table (context_simple, tmp_list->data, n_compose))
            return TRUE;
          tmp_list = tmp_list->next;
        }
  
      if (check_compact_table (context_simple, &gtk_compose_table_compact, n_compose))
        return TRUE;

      if (check_algorithmically (context_simple, n_compose))
	return TRUE;
    }
  
  /* The current compose_buffer doesn't match anything */
  return no_sequence_matches (context_simple, n_compose, event);
}

static void
gtk_im_context_simple_reset (GtkIMContext *context)
{
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);

  context_simple->compose_buffer[0] = 0;

  if (context_simple->tentative_match || context_simple->in_hex_sequence)
    {
      context_simple->in_hex_sequence = FALSE;
      context_simple->tentative_match = 0;
      context_simple->tentative_match_len = 0;
      g_signal_emit_by_name (context_simple, "preedit_changed");
      g_signal_emit_by_name (context_simple, "preedit_end");
    }
}

static void     
gtk_im_context_simple_get_preedit_string (GtkIMContext   *context,
					  gchar         **str,
					  PangoAttrList **attrs,
					  gint           *cursor_pos)
{
  char outbuf[37]; /* up to 6 hex digits */
  int len = 0;
  
  GtkIMContextSimple *context_simple = GTK_IM_CONTEXT_SIMPLE (context);

  if (context_simple->in_hex_sequence)
    {
      int hexchars = 0;
         
      outbuf[0] = 'u';
      len = 1;

      while (context_simple->compose_buffer[hexchars] != 0)
	{
	  len += g_unichar_to_utf8 (gdk_keyval_to_unicode (context_simple->compose_buffer[hexchars]),
				    outbuf + len);
	  ++hexchars;
	}

      g_assert (len < 25);
    }
  else if (context_simple->tentative_match)
    len = g_unichar_to_utf8 (context_simple->tentative_match, outbuf);
      
  outbuf[len] = '\0';      

  if (str)
    *str = g_strdup (outbuf);

  if (attrs)
    {
      *attrs = pango_attr_list_new ();
      
      if (len)
	{
	  PangoAttribute *attr = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
	  attr->start_index = 0;
          attr->end_index = len;
	  pango_attr_list_insert (*attrs, attr);
	}
    }

  if (cursor_pos)
    *cursor_pos = len;
}

/**
 * gtk_im_context_simple_add_table:
 * @context_simple: A #GtkIMContextSimple
 * @data: the table 
 * @max_seq_len: Maximum length of a sequence in the table
 *               (cannot be greater than #GTK_MAX_COMPOSE_LEN)
 * @n_seqs: number of sequences in the table
 * 
 * Adds an additional table to search to the input context.
 * Each row of the table consists of @max_seq_len key symbols
 * followed by two #guint16 interpreted as the high and low
 * words of a #gunicode value. Tables are searched starting
 * from the last added.
 *
 * The table must be sorted in dictionary order on the
 * numeric value of the key symbol fields. (Values beyond
 * the length of the sequence should be zero.)
 **/
void
gtk_im_context_simple_add_table (GtkIMContextSimple *context_simple,
				 guint16            *data,
				 gint                max_seq_len,
				 gint                n_seqs)
{
  GtkComposeTable *table;

  g_return_if_fail (GTK_IS_IM_CONTEXT_SIMPLE (context_simple));
  g_return_if_fail (data != NULL);
  g_return_if_fail (max_seq_len <= GTK_MAX_COMPOSE_LEN);
  
  table = g_new (GtkComposeTable, 1);
  table->data = data;
  table->max_seq_len = max_seq_len;
  table->n_seqs = n_seqs;

  context_simple->tables = g_slist_prepend (context_simple->tables, table);
}

#define __GTK_IM_CONTEXT_SIMPLE_C__
#include "gtkaliasdef.c"
