/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "gdk.h"

#include "gdkprivate-win32.h"
#include "gdkinternals.h"
#include "gdkkeysyms.h"

#include "config.h"

guint _gdk_keymap_serial = 0;

static GdkKeymap *default_keymap = NULL;

static guint *keysym_tab = NULL;

static void
update_keymap (void)
{
  static guint current_serial = 0;
  guchar key_state[256];
  guint scancode;
  guint vk;

  if (keysym_tab != NULL && current_serial == _gdk_keymap_serial)
    return;

  current_serial = _gdk_keymap_serial;

  if (keysym_tab == NULL)
    keysym_tab = g_new (guint, 4*256);

  memset (key_state, 0, sizeof (key_state));

  for (vk = 0; vk < 256; vk++)
    {
      if ((scancode = MapVirtualKey (vk, 0)) == 0)
	keysym_tab[vk*4+0] =
	  keysym_tab[vk*4+1] =
	  keysym_tab[vk*4+2] =
	  keysym_tab[vk*4+3] = GDK_VoidSymbol;
      else
	{
	  gint i, k;

	  key_state[vk] = 0x80;
	  for (i = 0; i < 4; i++)
	    {
	      guint *ksymp = keysym_tab + vk*4 + i;
	      guchar chars[2];
	      
	      switch (i)
		{
		case 0:
		  key_state[VK_SHIFT] = 0;
		  key_state[VK_CONTROL] = key_state[VK_MENU] = 0;
		  break;
		case 1:
		  key_state[VK_SHIFT] = 0x80;
		  key_state[VK_CONTROL] = key_state[VK_MENU] = 0;
		  break;
		case 2:
		  key_state[VK_SHIFT] = 0;
		  key_state[VK_CONTROL] = key_state[VK_MENU] = 0x80;
		  break;
		case 3:
		  key_state[VK_SHIFT] = 0x80;
		  key_state[VK_CONTROL] = key_state[VK_MENU] = 0x80;
		  break;
		}

	      *ksymp = 0;

	      /* First, handle those virtual keys that we always want
	       * as special GDK_* keysyms, even if ToAsciiEx might
	       * turn some them into a ASCII character (like TAB and
	       * ESC).
	       */
	      switch (vk)
		{
		case VK_CANCEL:
		  *ksymp = GDK_Cancel; break;
		case VK_BACK:
		  *ksymp = GDK_BackSpace; break;
		case VK_TAB:
		  if (i & 1)
		    *ksymp = GDK_ISO_Left_Tab;
		  else
		    *ksymp = GDK_Tab;
		  break;
		case VK_CLEAR:
		  *ksymp = GDK_Clear; break;
		case VK_RETURN:
		  *ksymp = GDK_Return; break;
		case VK_SHIFT:
		case VK_LSHIFT:
		  *ksymp = GDK_Shift_L; break;
		case VK_CONTROL:
		case VK_LCONTROL:
		  *ksymp = GDK_Control_L; break;
		case VK_MENU:
		case VK_LMENU:
		  *ksymp = GDK_Alt_L; break;
		case VK_PAUSE:
		  *ksymp = GDK_Pause; break;
		case VK_ESCAPE:
		  *ksymp = GDK_Escape; break;
		case VK_PRIOR:
		  *ksymp = GDK_Prior; break;
		case VK_NEXT:
		  *ksymp = GDK_Next; break;
		case VK_END:
		  *ksymp = GDK_End; break;
		case VK_HOME:
		  *ksymp = GDK_Home; break;
		case VK_LEFT:
		  *ksymp = GDK_Left; break;
		case VK_UP:
		  *ksymp = GDK_Up; break;
		case VK_RIGHT:
		  *ksymp = GDK_Right; break;
		case VK_DOWN:
		  *ksymp = GDK_Down; break;
		case VK_SELECT:
		  *ksymp = GDK_Select; break;
		case VK_PRINT:
		  *ksymp = GDK_Print; break;
		case VK_EXECUTE:
		  *ksymp = GDK_Execute; break;
		case VK_INSERT:
		  *ksymp = GDK_Insert; break;
		case VK_DELETE:
		  *ksymp = GDK_Delete; break;
		case VK_HELP:
		  *ksymp = GDK_Help; break;
		case VK_LWIN:
		  *ksymp = GDK_Meta_L; break;
		case VK_RWIN:
		  *ksymp = GDK_Meta_R; break;
		case VK_MULTIPLY:
		  *ksymp = GDK_KP_Multiply; break;
		case VK_ADD:
		  *ksymp = GDK_KP_Add; break;
		case VK_SEPARATOR:
		  *ksymp = GDK_KP_Separator; break;
		case VK_SUBTRACT:
		  *ksymp = GDK_KP_Subtract; break;
		case VK_DECIMAL:
		  *ksymp = GDK_KP_Decimal; break;
		case VK_DIVIDE:
		  *ksymp = GDK_KP_Divide; break;
		case VK_F1:
		  *ksymp = GDK_F1; break;
		case VK_F2:
		  *ksymp = GDK_F2; break;
		case VK_F3:
		  *ksymp = GDK_F3; break;
		case VK_F4:
		  *ksymp = GDK_F4; break;
		case VK_F5:
		  *ksymp = GDK_F5; break;
		case VK_F6:
		  *ksymp = GDK_F6; break;
		case VK_F7:
		  *ksymp = GDK_F7; break;
		case VK_F8:
		  *ksymp = GDK_F8; break;
		case VK_F9:
		  *ksymp = GDK_F9; break;
		case VK_F10:
		  *ksymp = GDK_F10; break;
		case VK_F11:
		  *ksymp = GDK_F11; break;
		case VK_F12:
		  *ksymp = GDK_F12; break;
		case VK_F13:
		  *ksymp = GDK_F13; break;
		case VK_F14:
		  *ksymp = GDK_F14; break;
		case VK_F15:
		  *ksymp = GDK_F15; break;
		case VK_F16:
		  *ksymp = GDK_F16; break;
		case VK_F17:
		  *ksymp = GDK_F17; break;
		case VK_F18:
		  *ksymp = GDK_F18; break;
		case VK_F19:
		  *ksymp = GDK_F19; break;
		case VK_F20:
		  *ksymp = GDK_F20; break;
		case VK_F21:
		  *ksymp = GDK_F21; break;
		case VK_F22:
		  *ksymp = GDK_F22; break;
		case VK_F23:
		  *ksymp = GDK_F23; break;
		case VK_F24:
		  *ksymp = GDK_F24; break;
		case VK_NUMLOCK:
		  *ksymp = GDK_Num_Lock; break;
		case VK_SCROLL:
		  *ksymp = GDK_Scroll_Lock; break;
		case VK_RSHIFT:
		  *ksymp = GDK_Shift_R; break;
		case VK_RCONTROL:
		  *ksymp = GDK_Control_R; break;
		case VK_RMENU:
		  *ksymp = GDK_Alt_R; break;
		}
	      if (*ksymp == 0)
		{
		  if ((k = ToAsciiEx (vk, scancode, key_state,
				      (LPWORD) chars, 0,
				      _gdk_input_locale)) == 1)
		    {
		      if (_gdk_input_codepage == 1252 &&
			  chars[0] >= GDK_space && chars[0] <= GDK_asciitilde)
			*ksymp = chars[0];
		      else
			{
			  wchar_t wcs[1];
			  if (MultiByteToWideChar (_gdk_input_codepage, 0,
						   chars, 1, wcs, 1) > 0)
			    *ksymp = gdk_unicode_to_keyval (wcs[0]);
			}
		    }
		}
	      if (*ksymp == 0)
		*ksymp = GDK_VoidSymbol;
	    }
	  key_state[vk] = 0;
	}
    }
#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
    {
      gint vk;

      g_print ("keymap:\n");
      for (vk = 0; vk < 256; vk++)
	{
	  gint state;

	  g_print ("%#.02x: ", vk);
	  for (state = 0; state < 4; state++)
	    {
	      gchar *name = gdk_keyval_name (keysym_tab[vk*4 + state]);
	      if (name == NULL)
		name = "(none)";
	      g_print ("%s ", name);
	    }
	  g_print ("\n");
	}
    }
#endif
} 

GdkKeymap*
gdk_keymap_get_default (void)
{
  if (default_keymap == NULL)
    default_keymap = g_object_new (gdk_keymap_get_type (), NULL);

  return default_keymap;
}

PangoDirection
gdk_keymap_get_direction (GdkKeymap *keymap)
{
  update_keymap ();

  switch (PRIMARYLANGID (LOWORD ((DWORD) _gdk_input_locale)))
    {
    case LANG_HEBREW:
    case LANG_ARABIC:
      /* Not 100% sure about these */
#ifdef LANG_URDU
    case LANG_URDU:
#endif
    case LANG_FARSI:
      /* Others? */
      return PANGO_DIRECTION_RTL;

    default:
      return PANGO_DIRECTION_LTR;
    }
}

/**
 * gdk_keymap_get_entries_for_keyval:
 * @keymap: a #GdkKeymap, or %NULL to use the default keymap
 * @keyval: a keyval, such as %GDK_a, %GDK_Up, %GDK_Return, etc.
 * @keys: return location for an array of #GdkKeymapKey
 * @n_keys: return location for number of elements in returned array
 * 
 * Obtains a list of keycode/group/level combinations that will
 * generate @keyval. Groups and levels are two kinds of keyboard mode;
 * in general, the level determines whether the top or bottom symbol
 * on a key is used, and the group determines whether the left or
 * right symbol is used. On US keyboards, the shift key changes the
 * keyboard level, and there are no groups. A group switch key might
 * convert a keyboard between Hebrew to English modes, for example.
 * #GdkEventKey contains a %group field that indicates the active
 * keyboard group. The level is computed from the modifier mask.
 * The returned array should be freed
 * with g_free().
 *
 * Return value: %TRUE if keys were found and returned
 **/
gboolean
gdk_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                   guint          keyval,
                                   GdkKeymapKey **keys,
                                   gint          *n_keys)
{
  GArray *retval;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (keys != NULL, FALSE);
  g_return_val_if_fail (n_keys != NULL, FALSE);
  g_return_val_if_fail (keyval != 0, FALSE);
  
  retval = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  /* Accept only the default keymap */
  if (keymap == NULL || keymap == gdk_keymap_get_default ())
    {
      gint vk;
      
      update_keymap ();

      for (vk = 0; vk < 256; vk++)
	{
	  gint i;

	  for (i = 0; i < 4; i++)
	    {
	      if (keysym_tab[vk*4+i] == keyval)
		{
		  GdkKeymapKey key;
		  
		  key.keycode = vk;
		  
		  /* 2 levels (normal, shift), two groups (normal, AltGr) */
		  key.group = i / 2;
		  key.level = i % 2;
		  
		  g_array_append_val (retval, key);
		}
	    }
	}
    }

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
    {
      gint i;

      g_print ("gdk_keymap_get_entries_for_keyval: %#.04x (%s):",
	       keyval, gdk_keyval_name (keyval));
      for (i = 0; i < retval->len; i++)
	{
	  GdkKeymapKey *entry = (GdkKeymapKey *) retval->data + i;
	  g_print ("  %#.02x %d %d", entry->keycode, entry->group, entry->level);
	}
      g_print ("\n");
    }
#endif

  if (retval->len > 0)
    {
      *keys = (GdkKeymapKey*) retval->data;
      *n_keys = retval->len;
    }
  else
    {
      *keys = NULL;
      *n_keys = 0;
    }
      
  g_array_free (retval, retval->len > 0 ? FALSE : TRUE);

  return *n_keys > 0;
}

/**
 * gdk_keymap_get_entries_for_keycode:
 * @keymap: a #GdkKeymap or %NULL to use the default keymap
 * @hardware_keycode: a keycode
 * @keys: return location for array of #GdkKeymapKey, or NULL
 * @keyvals: return location for array of keyvals, or NULL
 * @n_entries: length of @keys and @keyvals
 *
 * Returns the keyvals bound to @hardware_keycode.
 * The Nth #GdkKeymapKey in @keys is bound to the Nth
 * keyval in @keyvals. Free the returned arrays with g_free().
 * When a keycode is pressed by the user, the keyval from
 * this list of entries is selected by considering the effective
 * keyboard group and level. See gdk_keymap_translate_keyboard_state().
 *
 * Returns: %TRUE if there were any entries
 **/
gboolean
gdk_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                    guint          hardware_keycode,
                                    GdkKeymapKey **keys,
                                    guint        **keyvals,
                                    gint          *n_entries)
{
  GArray *key_array;
  GArray *keyval_array;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

  if (hardware_keycode <= 0 ||
      hardware_keycode >= 256)
    {
      if (keys)
        *keys = NULL;
      if (keyvals)
        *keyvals = NULL;

      *n_entries = 0;
      return FALSE;
    }
  
  if (keys)
    key_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  else
    key_array = NULL;
  
  if (keyvals)
    keyval_array = g_array_new (FALSE, FALSE, sizeof (guint));
  else
    keyval_array = NULL;
  
  /* Accept only the default keymap */
  if (keymap == NULL || keymap == gdk_keymap_get_default ())
    {
      gint i;

      update_keymap ();

      for (i = 0; i < 4; i++)
	{
	  if (key_array)
            {
              GdkKeymapKey key;
	      
              key.keycode = hardware_keycode;
              
              key.group = i / 2;
              key.level = i % 2;
              
              g_array_append_val (key_array, key);
            }

          if (keyval_array)
            g_array_append_val (keyval_array, keysym_tab[hardware_keycode*4+i]);
	}
    }

  if ((key_array && key_array->len > 0) ||
      (keyval_array && keyval_array->len > 0))
    {
      if (keys)
        *keys = (GdkKeymapKey*) key_array->data;

      if (keyvals)
        *keyvals = (guint*) keyval_array->data;

      if (key_array)
        *n_entries = key_array->len;
      else
        *n_entries = keyval_array->len;
    }
  else
    {
      if (keys)
        *keys = NULL;

      if (keyvals)
        *keyvals = NULL;
      
      *n_entries = 0;
    }

  if (key_array)
    g_array_free (key_array, key_array->len > 0 ? FALSE : TRUE);
  if (keyval_array)
    g_array_free (keyval_array, keyval_array->len > 0 ? FALSE : TRUE);

  return *n_entries > 0;
}


/**
 * gdk_keymap_lookup_key:
 * @keymap: a #GdkKeymap or %NULL to use the default keymap
 * @key: a #GdkKeymapKey with keycode, group, and level initialized
 * 
 * Looks up the keyval mapped to a keycode/group/level triplet.
 * If no keyval is bound to @key, returns 0. For normal user input,
 * you want to use gdk_keymap_translate_keyboard_state() instead of
 * this function, since the effective group/level may not be
 * the same as the current keyboard state.
 * 
 * Return value: a keyval, or 0 if none was mapped to the given @key
 **/
guint
gdk_keymap_lookup_key (GdkKeymap          *keymap,
                       const GdkKeymapKey *key)
{
  guint sym;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (key->group < 4, 0);
  
  /* Accept only the default keymap */
  if (keymap != NULL && keymap != gdk_keymap_get_default ())
    return 0;

  update_keymap ();
  
  if (key->keycode >= 256 ||
      key->group < 0 || key->group >= 2 ||
      key->level < 0 || key->level >= 2)
    return 0;
  
  sym = keysym_tab[key->keycode*4 + key->group*2 + key->level];
  
  if (sym == GDK_VoidSymbol)
    return 0;
  else
    return sym;
}

/**
 * gdk_keymap_translate_keyboard_state:
 * @keymap: a #GdkKeymap, or %NULL to use the default
 * @hardware_keycode: a keycode
 * @state: a modifier state 
 * @group: active keyboard group
 * @keyval: return location for keyval
 * @effective_group: return location for effective group
 * @level: return location for level
 * @consumed_modifiers: return location for modifiers that were used to determine the group or level
 * 
 *
 * Translates the contents of a #GdkEventKey into a keyval, effective
 * group, and level. Modifiers that affected the translation and
 * are thus unavailable for application use are returned in
 * @consumed_modifiers.  See gdk_keyval_get_keys() for an explanation of
 * groups and levels.  The @effective_group is the group that was
 * actually used for the translation; some keys such as Enter are not
 * affected by the active keyboard group. The @level is derived from
 * @state. For convenience, #GdkEventKey already contains the translated
 * keyval, so this function isn't as useful as you might think.
 * 
 * Return value: %TRUE if there was a keyval bound to the keycode/state/group
 **/
gboolean
gdk_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                     guint            hardware_keycode,
                                     GdkModifierType  state,
                                     gint             group,
                                     guint           *keyval,
                                     gint            *effective_group,
                                     gint            *level,
                                     GdkModifierType *consumed_modifiers)
{
  guint tmp_keyval;
  guint tmp_modifiers;
  guint *keyvals;
  gint shift_level;
  gboolean ignore_shift = FALSE;
  gboolean ignore_group = FALSE;
      
  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (group < 4, FALSE);
  
  GDK_NOTE (EVENTS, g_print ("gdk_keymap_translate_keyboard_state: keycode=%#x state=%#x group=%d\n",
			     hardware_keycode, state, group));

  if (keyval)
    *keyval = 0;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  /* Accept only the default keymap */
  if (keymap != NULL && keymap != gdk_keymap_get_default ())
    return FALSE;

  if (hardware_keycode >= 256)
    return FALSE;

  if (group < 0 || group >= 2)
    return FALSE;

  if ((state & GDK_SHIFT_MASK) && (state & GDK_LOCK_MASK))
    shift_level = 0;		/* shift disables shift lock */
  else if ((state & GDK_SHIFT_MASK) || (state & GDK_LOCK_MASK))
    shift_level = 1;
  else
    shift_level = 0;

  update_keymap ();

  keyvals = keysym_tab + hardware_keycode*4;

  /* Drop group and shift if there are no keysymbols on
   * the key for those.
   */
  if (shift_level == 1 &&
      keyvals[group*2 + 1] == GDK_VoidSymbol &&
      keyvals[group*2] != GDK_VoidSymbol)
    {
      shift_level = 0;
      ignore_shift = TRUE;
    }

  if (group == 1 &&
      keyvals[2 + shift_level] == GDK_VoidSymbol &&
      keyvals[0 + shift_level] != GDK_VoidSymbol)
    {
      group = 0;
      ignore_group = TRUE;
    }

  if (keyvals[group *2 + shift_level] == GDK_VoidSymbol &&
      keyvals[0 + 0] != GDK_VoidSymbol)
    {
      shift_level = 0;
      group = 0;
      ignore_group = TRUE;
      ignore_shift = TRUE;
    }

  /* See whether the group and shift level actually mattered
   * to know what to put in consumed_modifiers
   */
  if (keyvals[group*2 + 1] == GDK_VoidSymbol ||
      keyvals[group*2 + 0] == keyvals[group*2 + 1])
    ignore_shift = TRUE;

  if (keyvals[2 + shift_level] == GDK_VoidSymbol ||
      keyvals[0 + shift_level] == keyvals[2 + shift_level])
    ignore_group = TRUE;

  tmp_keyval = keyvals[group*2 + shift_level];

  tmp_modifiers = ignore_group ? 0 : GDK_MOD2_MASK;
  tmp_modifiers |= ignore_shift ? 0 : (GDK_SHIFT_MASK | GDK_LOCK_MASK);

  if (effective_group)
    *effective_group = group;

  if (level)
    *level = shift_level;

  if (consumed_modifiers)
    *consumed_modifiers = tmp_modifiers;
				
  if (keyval)
    *keyval = tmp_keyval;

  GDK_NOTE (EVENTS, g_print ("...group=%d level=%d cmods=%#x keyval=%s\n",
			     group, shift_level, tmp_modifiers, gdk_keyval_name (tmp_keyval)));

  return tmp_keyval != GDK_VoidSymbol;
}

/* Key handling not part of the keymap */

static struct gdk_key {
  guint keyval;
  const char *name;
} gdk_keys_by_keyval[] = {
  { 0x000020, "space" },
  { 0x000021, "exclam" },
  { 0x000022, "quotedbl" },
  { 0x000023, "numbersign" },
  { 0x000024, "dollar" },
  { 0x000025, "percent" },
  { 0x000026, "ampersand" },
  { 0x000027, "apostrophe" },
  { 0x000027, "quoteright" },
  { 0x000028, "parenleft" },
  { 0x000029, "parenright" },
  { 0x00002a, "asterisk" },
  { 0x00002b, "plus" },
  { 0x00002c, "comma" },
  { 0x00002d, "minus" },
  { 0x00002e, "period" },
  { 0x00002f, "slash" },
  { 0x000030, "0" },
  { 0x000031, "1" },
  { 0x000032, "2" },
  { 0x000033, "3" },
  { 0x000034, "4" },
  { 0x000035, "5" },
  { 0x000036, "6" },
  { 0x000037, "7" },
  { 0x000038, "8" },
  { 0x000039, "9" },
  { 0x00003a, "colon" },
  { 0x00003b, "semicolon" },
  { 0x00003c, "less" },
  { 0x00003d, "equal" },
  { 0x00003e, "greater" },
  { 0x00003f, "question" },
  { 0x000040, "at" },
  { 0x000041, "A" },
  { 0x000042, "B" },
  { 0x000043, "C" },
  { 0x000044, "D" },
  { 0x000045, "E" },
  { 0x000046, "F" },
  { 0x000047, "G" },
  { 0x000048, "H" },
  { 0x000049, "I" },
  { 0x00004a, "J" },
  { 0x00004b, "K" },
  { 0x00004c, "L" },
  { 0x00004d, "M" },
  { 0x00004e, "N" },
  { 0x00004f, "O" },
  { 0x000050, "P" },
  { 0x000051, "Q" },
  { 0x000052, "R" },
  { 0x000053, "S" },
  { 0x000054, "T" },
  { 0x000055, "U" },
  { 0x000056, "V" },
  { 0x000057, "W" },
  { 0x000058, "X" },
  { 0x000059, "Y" },
  { 0x00005a, "Z" },
  { 0x00005b, "bracketleft" },
  { 0x00005c, "backslash" },
  { 0x00005d, "bracketright" },
  { 0x00005e, "asciicircum" },
  { 0x00005f, "underscore" },
  { 0x000060, "grave" },
  { 0x000060, "quoteleft" },
  { 0x000061, "a" },
  { 0x000062, "b" },
  { 0x000063, "c" },
  { 0x000064, "d" },
  { 0x000065, "e" },
  { 0x000066, "f" },
  { 0x000067, "g" },
  { 0x000068, "h" },
  { 0x000069, "i" },
  { 0x00006a, "j" },
  { 0x00006b, "k" },
  { 0x00006c, "l" },
  { 0x00006d, "m" },
  { 0x00006e, "n" },
  { 0x00006f, "o" },
  { 0x000070, "p" },
  { 0x000071, "q" },
  { 0x000072, "r" },
  { 0x000073, "s" },
  { 0x000074, "t" },
  { 0x000075, "u" },
  { 0x000076, "v" },
  { 0x000077, "w" },
  { 0x000078, "x" },
  { 0x000079, "y" },
  { 0x00007a, "z" },
  { 0x00007b, "braceleft" },
  { 0x00007c, "bar" },
  { 0x00007d, "braceright" },
  { 0x00007e, "asciitilde" },
  { 0x0000a0, "nobreakspace" },
  { 0x0000a1, "exclamdown" },
  { 0x0000a2, "cent" },
  { 0x0000a3, "sterling" },
  { 0x0000a4, "currency" },
  { 0x0000a5, "yen" },
  { 0x0000a6, "brokenbar" },
  { 0x0000a7, "section" },
  { 0x0000a8, "diaeresis" },
  { 0x0000a9, "copyright" },
  { 0x0000aa, "ordfeminine" },
  { 0x0000ab, "guillemotleft" },
  { 0x0000ac, "notsign" },
  { 0x0000ad, "hyphen" },
  { 0x0000ae, "registered" },
  { 0x0000af, "macron" },
  { 0x0000b0, "degree" },
  { 0x0000b1, "plusminus" },
  { 0x0000b2, "twosuperior" },
  { 0x0000b3, "threesuperior" },
  { 0x0000b4, "acute" },
  { 0x0000b5, "mu" },
  { 0x0000b6, "paragraph" },
  { 0x0000b7, "periodcentered" },
  { 0x0000b8, "cedilla" },
  { 0x0000b9, "onesuperior" },
  { 0x0000ba, "masculine" },
  { 0x0000bb, "guillemotright" },
  { 0x0000bc, "onequarter" },
  { 0x0000bd, "onehalf" },
  { 0x0000be, "threequarters" },
  { 0x0000bf, "questiondown" },
  { 0x0000c0, "Agrave" },
  { 0x0000c1, "Aacute" },
  { 0x0000c2, "Acircumflex" },
  { 0x0000c3, "Atilde" },
  { 0x0000c4, "Adiaeresis" },
  { 0x0000c5, "Aring" },
  { 0x0000c6, "AE" },
  { 0x0000c7, "Ccedilla" },
  { 0x0000c8, "Egrave" },
  { 0x0000c9, "Eacute" },
  { 0x0000ca, "Ecircumflex" },
  { 0x0000cb, "Ediaeresis" },
  { 0x0000cc, "Igrave" },
  { 0x0000cd, "Iacute" },
  { 0x0000ce, "Icircumflex" },
  { 0x0000cf, "Idiaeresis" },
  { 0x0000d0, "ETH" },
  { 0x0000d0, "Eth" },
  { 0x0000d1, "Ntilde" },
  { 0x0000d2, "Ograve" },
  { 0x0000d3, "Oacute" },
  { 0x0000d4, "Ocircumflex" },
  { 0x0000d5, "Otilde" },
  { 0x0000d6, "Odiaeresis" },
  { 0x0000d7, "multiply" },
  { 0x0000d8, "Ooblique" },
  { 0x0000d9, "Ugrave" },
  { 0x0000da, "Uacute" },
  { 0x0000db, "Ucircumflex" },
  { 0x0000dc, "Udiaeresis" },
  { 0x0000dd, "Yacute" },
  { 0x0000de, "THORN" },
  { 0x0000de, "Thorn" },
  { 0x0000df, "ssharp" },
  { 0x0000e0, "agrave" },
  { 0x0000e1, "aacute" },
  { 0x0000e2, "acircumflex" },
  { 0x0000e3, "atilde" },
  { 0x0000e4, "adiaeresis" },
  { 0x0000e5, "aring" },
  { 0x0000e6, "ae" },
  { 0x0000e7, "ccedilla" },
  { 0x0000e8, "egrave" },
  { 0x0000e9, "eacute" },
  { 0x0000ea, "ecircumflex" },
  { 0x0000eb, "ediaeresis" },
  { 0x0000ec, "igrave" },
  { 0x0000ed, "iacute" },
  { 0x0000ee, "icircumflex" },
  { 0x0000ef, "idiaeresis" },
  { 0x0000f0, "eth" },
  { 0x0000f1, "ntilde" },
  { 0x0000f2, "ograve" },
  { 0x0000f3, "oacute" },
  { 0x0000f4, "ocircumflex" },
  { 0x0000f5, "otilde" },
  { 0x0000f6, "odiaeresis" },
  { 0x0000f7, "division" },
  { 0x0000f8, "oslash" },
  { 0x0000f9, "ugrave" },
  { 0x0000fa, "uacute" },
  { 0x0000fb, "ucircumflex" },
  { 0x0000fc, "udiaeresis" },
  { 0x0000fd, "yacute" },
  { 0x0000fe, "thorn" },
  { 0x0000ff, "ydiaeresis" },
  { 0x0001a1, "Aogonek" },
  { 0x0001a2, "breve" },
  { 0x0001a3, "Lstroke" },
  { 0x0001a5, "Lcaron" },
  { 0x0001a6, "Sacute" },
  { 0x0001a9, "Scaron" },
  { 0x0001aa, "Scedilla" },
  { 0x0001ab, "Tcaron" },
  { 0x0001ac, "Zacute" },
  { 0x0001ae, "Zcaron" },
  { 0x0001af, "Zabovedot" },
  { 0x0001b1, "aogonek" },
  { 0x0001b2, "ogonek" },
  { 0x0001b3, "lstroke" },
  { 0x0001b5, "lcaron" },
  { 0x0001b6, "sacute" },
  { 0x0001b7, "caron" },
  { 0x0001b9, "scaron" },
  { 0x0001ba, "scedilla" },
  { 0x0001bb, "tcaron" },
  { 0x0001bc, "zacute" },
  { 0x0001bd, "doubleacute" },
  { 0x0001be, "zcaron" },
  { 0x0001bf, "zabovedot" },
  { 0x0001c0, "Racute" },
  { 0x0001c3, "Abreve" },
  { 0x0001c5, "Lacute" },
  { 0x0001c6, "Cacute" },
  { 0x0001c8, "Ccaron" },
  { 0x0001ca, "Eogonek" },
  { 0x0001cc, "Ecaron" },
  { 0x0001cf, "Dcaron" },
  { 0x0001d0, "Dstroke" },
  { 0x0001d1, "Nacute" },
  { 0x0001d2, "Ncaron" },
  { 0x0001d5, "Odoubleacute" },
  { 0x0001d8, "Rcaron" },
  { 0x0001d9, "Uring" },
  { 0x0001db, "Udoubleacute" },
  { 0x0001de, "Tcedilla" },
  { 0x0001e0, "racute" },
  { 0x0001e3, "abreve" },
  { 0x0001e5, "lacute" },
  { 0x0001e6, "cacute" },
  { 0x0001e8, "ccaron" },
  { 0x0001ea, "eogonek" },
  { 0x0001ec, "ecaron" },
  { 0x0001ef, "dcaron" },
  { 0x0001f0, "dstroke" },
  { 0x0001f1, "nacute" },
  { 0x0001f2, "ncaron" },
  { 0x0001f5, "odoubleacute" },
  { 0x0001f8, "rcaron" },
  { 0x0001f9, "uring" },
  { 0x0001fb, "udoubleacute" },
  { 0x0001fe, "tcedilla" },
  { 0x0001ff, "abovedot" },
  { 0x0002a1, "Hstroke" },
  { 0x0002a6, "Hcircumflex" },
  { 0x0002a9, "Iabovedot" },
  { 0x0002ab, "Gbreve" },
  { 0x0002ac, "Jcircumflex" },
  { 0x0002b1, "hstroke" },
  { 0x0002b6, "hcircumflex" },
  { 0x0002b9, "idotless" },
  { 0x0002bb, "gbreve" },
  { 0x0002bc, "jcircumflex" },
  { 0x0002c5, "Cabovedot" },
  { 0x0002c6, "Ccircumflex" },
  { 0x0002d5, "Gabovedot" },
  { 0x0002d8, "Gcircumflex" },
  { 0x0002dd, "Ubreve" },
  { 0x0002de, "Scircumflex" },
  { 0x0002e5, "cabovedot" },
  { 0x0002e6, "ccircumflex" },
  { 0x0002f5, "gabovedot" },
  { 0x0002f8, "gcircumflex" },
  { 0x0002fd, "ubreve" },
  { 0x0002fe, "scircumflex" },
  { 0x0003a2, "kappa" },
  { 0x0003a2, "kra" },
  { 0x0003a3, "Rcedilla" },
  { 0x0003a5, "Itilde" },
  { 0x0003a6, "Lcedilla" },
  { 0x0003aa, "Emacron" },
  { 0x0003ab, "Gcedilla" },
  { 0x0003ac, "Tslash" },
  { 0x0003b3, "rcedilla" },
  { 0x0003b5, "itilde" },
  { 0x0003b6, "lcedilla" },
  { 0x0003ba, "emacron" },
  { 0x0003bb, "gcedilla" },
  { 0x0003bc, "tslash" },
  { 0x0003bd, "ENG" },
  { 0x0003bf, "eng" },
  { 0x0003c0, "Amacron" },
  { 0x0003c7, "Iogonek" },
  { 0x0003cc, "Eabovedot" },
  { 0x0003cf, "Imacron" },
  { 0x0003d1, "Ncedilla" },
  { 0x0003d2, "Omacron" },
  { 0x0003d3, "Kcedilla" },
  { 0x0003d9, "Uogonek" },
  { 0x0003dd, "Utilde" },
  { 0x0003de, "Umacron" },
  { 0x0003e0, "amacron" },
  { 0x0003e7, "iogonek" },
  { 0x0003ec, "eabovedot" },
  { 0x0003ef, "imacron" },
  { 0x0003f1, "ncedilla" },
  { 0x0003f2, "omacron" },
  { 0x0003f3, "kcedilla" },
  { 0x0003f9, "uogonek" },
  { 0x0003fd, "utilde" },
  { 0x0003fe, "umacron" },
  { 0x00047e, "overline" },
  { 0x0004a1, "kana_fullstop" },
  { 0x0004a2, "kana_openingbracket" },
  { 0x0004a3, "kana_closingbracket" },
  { 0x0004a4, "kana_comma" },
  { 0x0004a5, "kana_conjunctive" },
  { 0x0004a5, "kana_middledot" },
  { 0x0004a6, "kana_WO" },
  { 0x0004a7, "kana_a" },
  { 0x0004a8, "kana_i" },
  { 0x0004a9, "kana_u" },
  { 0x0004aa, "kana_e" },
  { 0x0004ab, "kana_o" },
  { 0x0004ac, "kana_ya" },
  { 0x0004ad, "kana_yu" },
  { 0x0004ae, "kana_yo" },
  { 0x0004af, "kana_tsu" },
  { 0x0004af, "kana_tu" },
  { 0x0004b0, "prolongedsound" },
  { 0x0004b1, "kana_A" },
  { 0x0004b2, "kana_I" },
  { 0x0004b3, "kana_U" },
  { 0x0004b4, "kana_E" },
  { 0x0004b5, "kana_O" },
  { 0x0004b6, "kana_KA" },
  { 0x0004b7, "kana_KI" },
  { 0x0004b8, "kana_KU" },
  { 0x0004b9, "kana_KE" },
  { 0x0004ba, "kana_KO" },
  { 0x0004bb, "kana_SA" },
  { 0x0004bc, "kana_SHI" },
  { 0x0004bd, "kana_SU" },
  { 0x0004be, "kana_SE" },
  { 0x0004bf, "kana_SO" },
  { 0x0004c0, "kana_TA" },
  { 0x0004c1, "kana_CHI" },
  { 0x0004c1, "kana_TI" },
  { 0x0004c2, "kana_TSU" },
  { 0x0004c2, "kana_TU" },
  { 0x0004c3, "kana_TE" },
  { 0x0004c4, "kana_TO" },
  { 0x0004c5, "kana_NA" },
  { 0x0004c6, "kana_NI" },
  { 0x0004c7, "kana_NU" },
  { 0x0004c8, "kana_NE" },
  { 0x0004c9, "kana_NO" },
  { 0x0004ca, "kana_HA" },
  { 0x0004cb, "kana_HI" },
  { 0x0004cc, "kana_FU" },
  { 0x0004cc, "kana_HU" },
  { 0x0004cd, "kana_HE" },
  { 0x0004ce, "kana_HO" },
  { 0x0004cf, "kana_MA" },
  { 0x0004d0, "kana_MI" },
  { 0x0004d1, "kana_MU" },
  { 0x0004d2, "kana_ME" },
  { 0x0004d3, "kana_MO" },
  { 0x0004d4, "kana_YA" },
  { 0x0004d5, "kana_YU" },
  { 0x0004d6, "kana_YO" },
  { 0x0004d7, "kana_RA" },
  { 0x0004d8, "kana_RI" },
  { 0x0004d9, "kana_RU" },
  { 0x0004da, "kana_RE" },
  { 0x0004db, "kana_RO" },
  { 0x0004dc, "kana_WA" },
  { 0x0004dd, "kana_N" },
  { 0x0004de, "voicedsound" },
  { 0x0004df, "semivoicedsound" },
  { 0x0005ac, "Arabic_comma" },
  { 0x0005bb, "Arabic_semicolon" },
  { 0x0005bf, "Arabic_question_mark" },
  { 0x0005c1, "Arabic_hamza" },
  { 0x0005c2, "Arabic_maddaonalef" },
  { 0x0005c3, "Arabic_hamzaonalef" },
  { 0x0005c4, "Arabic_hamzaonwaw" },
  { 0x0005c5, "Arabic_hamzaunderalef" },
  { 0x0005c6, "Arabic_hamzaonyeh" },
  { 0x0005c7, "Arabic_alef" },
  { 0x0005c8, "Arabic_beh" },
  { 0x0005c9, "Arabic_tehmarbuta" },
  { 0x0005ca, "Arabic_teh" },
  { 0x0005cb, "Arabic_theh" },
  { 0x0005cc, "Arabic_jeem" },
  { 0x0005cd, "Arabic_hah" },
  { 0x0005ce, "Arabic_khah" },
  { 0x0005cf, "Arabic_dal" },
  { 0x0005d0, "Arabic_thal" },
  { 0x0005d1, "Arabic_ra" },
  { 0x0005d2, "Arabic_zain" },
  { 0x0005d3, "Arabic_seen" },
  { 0x0005d4, "Arabic_sheen" },
  { 0x0005d5, "Arabic_sad" },
  { 0x0005d6, "Arabic_dad" },
  { 0x0005d7, "Arabic_tah" },
  { 0x0005d8, "Arabic_zah" },
  { 0x0005d9, "Arabic_ain" },
  { 0x0005da, "Arabic_ghain" },
  { 0x0005e0, "Arabic_tatweel" },
  { 0x0005e1, "Arabic_feh" },
  { 0x0005e2, "Arabic_qaf" },
  { 0x0005e3, "Arabic_kaf" },
  { 0x0005e4, "Arabic_lam" },
  { 0x0005e5, "Arabic_meem" },
  { 0x0005e6, "Arabic_noon" },
  { 0x0005e7, "Arabic_ha" },
  { 0x0005e7, "Arabic_heh" },
  { 0x0005e8, "Arabic_waw" },
  { 0x0005e9, "Arabic_alefmaksura" },
  { 0x0005ea, "Arabic_yeh" },
  { 0x0005eb, "Arabic_fathatan" },
  { 0x0005ec, "Arabic_dammatan" },
  { 0x0005ed, "Arabic_kasratan" },
  { 0x0005ee, "Arabic_fatha" },
  { 0x0005ef, "Arabic_damma" },
  { 0x0005f0, "Arabic_kasra" },
  { 0x0005f1, "Arabic_shadda" },
  { 0x0005f2, "Arabic_sukun" },
  { 0x0006a1, "Serbian_dje" },
  { 0x0006a2, "Macedonia_gje" },
  { 0x0006a3, "Cyrillic_io" },
  { 0x0006a4, "Ukrainian_ie" },
  { 0x0006a4, "Ukranian_je" },
  { 0x0006a5, "Macedonia_dse" },
  { 0x0006a6, "Ukrainian_i" },
  { 0x0006a6, "Ukranian_i" },
  { 0x0006a7, "Ukrainian_yi" },
  { 0x0006a7, "Ukranian_yi" },
  { 0x0006a8, "Cyrillic_je" },
  { 0x0006a8, "Serbian_je" },
  { 0x0006a9, "Cyrillic_lje" },
  { 0x0006a9, "Serbian_lje" },
  { 0x0006aa, "Cyrillic_nje" },
  { 0x0006aa, "Serbian_nje" },
  { 0x0006ab, "Serbian_tshe" },
  { 0x0006ac, "Macedonia_kje" },
  { 0x0006ae, "Byelorussian_shortu" },
  { 0x0006af, "Cyrillic_dzhe" },
  { 0x0006af, "Serbian_dze" },
  { 0x0006b0, "numerosign" },
  { 0x0006b1, "Serbian_DJE" },
  { 0x0006b2, "Macedonia_GJE" },
  { 0x0006b3, "Cyrillic_IO" },
  { 0x0006b4, "Ukrainian_IE" },
  { 0x0006b4, "Ukranian_JE" },
  { 0x0006b5, "Macedonia_DSE" },
  { 0x0006b6, "Ukrainian_I" },
  { 0x0006b6, "Ukranian_I" },
  { 0x0006b7, "Ukrainian_YI" },
  { 0x0006b7, "Ukranian_YI" },
  { 0x0006b8, "Cyrillic_JE" },
  { 0x0006b8, "Serbian_JE" },
  { 0x0006b9, "Cyrillic_LJE" },
  { 0x0006b9, "Serbian_LJE" },
  { 0x0006ba, "Cyrillic_NJE" },
  { 0x0006ba, "Serbian_NJE" },
  { 0x0006bb, "Serbian_TSHE" },
  { 0x0006bc, "Macedonia_KJE" },
  { 0x0006be, "Byelorussian_SHORTU" },
  { 0x0006bf, "Cyrillic_DZHE" },
  { 0x0006bf, "Serbian_DZE" },
  { 0x0006c0, "Cyrillic_yu" },
  { 0x0006c1, "Cyrillic_a" },
  { 0x0006c2, "Cyrillic_be" },
  { 0x0006c3, "Cyrillic_tse" },
  { 0x0006c4, "Cyrillic_de" },
  { 0x0006c5, "Cyrillic_ie" },
  { 0x0006c6, "Cyrillic_ef" },
  { 0x0006c7, "Cyrillic_ghe" },
  { 0x0006c8, "Cyrillic_ha" },
  { 0x0006c9, "Cyrillic_i" },
  { 0x0006ca, "Cyrillic_shorti" },
  { 0x0006cb, "Cyrillic_ka" },
  { 0x0006cc, "Cyrillic_el" },
  { 0x0006cd, "Cyrillic_em" },
  { 0x0006ce, "Cyrillic_en" },
  { 0x0006cf, "Cyrillic_o" },
  { 0x0006d0, "Cyrillic_pe" },
  { 0x0006d1, "Cyrillic_ya" },
  { 0x0006d2, "Cyrillic_er" },
  { 0x0006d3, "Cyrillic_es" },
  { 0x0006d4, "Cyrillic_te" },
  { 0x0006d5, "Cyrillic_u" },
  { 0x0006d6, "Cyrillic_zhe" },
  { 0x0006d7, "Cyrillic_ve" },
  { 0x0006d8, "Cyrillic_softsign" },
  { 0x0006d9, "Cyrillic_yeru" },
  { 0x0006da, "Cyrillic_ze" },
  { 0x0006db, "Cyrillic_sha" },
  { 0x0006dc, "Cyrillic_e" },
  { 0x0006dd, "Cyrillic_shcha" },
  { 0x0006de, "Cyrillic_che" },
  { 0x0006df, "Cyrillic_hardsign" },
  { 0x0006e0, "Cyrillic_YU" },
  { 0x0006e1, "Cyrillic_A" },
  { 0x0006e2, "Cyrillic_BE" },
  { 0x0006e3, "Cyrillic_TSE" },
  { 0x0006e4, "Cyrillic_DE" },
  { 0x0006e5, "Cyrillic_IE" },
  { 0x0006e6, "Cyrillic_EF" },
  { 0x0006e7, "Cyrillic_GHE" },
  { 0x0006e8, "Cyrillic_HA" },
  { 0x0006e9, "Cyrillic_I" },
  { 0x0006ea, "Cyrillic_SHORTI" },
  { 0x0006eb, "Cyrillic_KA" },
  { 0x0006ec, "Cyrillic_EL" },
  { 0x0006ed, "Cyrillic_EM" },
  { 0x0006ee, "Cyrillic_EN" },
  { 0x0006ef, "Cyrillic_O" },
  { 0x0006f0, "Cyrillic_PE" },
  { 0x0006f1, "Cyrillic_YA" },
  { 0x0006f2, "Cyrillic_ER" },
  { 0x0006f3, "Cyrillic_ES" },
  { 0x0006f4, "Cyrillic_TE" },
  { 0x0006f5, "Cyrillic_U" },
  { 0x0006f6, "Cyrillic_ZHE" },
  { 0x0006f7, "Cyrillic_VE" },
  { 0x0006f8, "Cyrillic_SOFTSIGN" },
  { 0x0006f9, "Cyrillic_YERU" },
  { 0x0006fa, "Cyrillic_ZE" },
  { 0x0006fb, "Cyrillic_SHA" },
  { 0x0006fc, "Cyrillic_E" },
  { 0x0006fd, "Cyrillic_SHCHA" },
  { 0x0006fe, "Cyrillic_CHE" },
  { 0x0006ff, "Cyrillic_HARDSIGN" },
  { 0x0007a1, "Greek_ALPHAaccent" },
  { 0x0007a2, "Greek_EPSILONaccent" },
  { 0x0007a3, "Greek_ETAaccent" },
  { 0x0007a4, "Greek_IOTAaccent" },
  { 0x0007a5, "Greek_IOTAdiaeresis" },
  { 0x0007a7, "Greek_OMICRONaccent" },
  { 0x0007a8, "Greek_UPSILONaccent" },
  { 0x0007a9, "Greek_UPSILONdieresis" },
  { 0x0007ab, "Greek_OMEGAaccent" },
  { 0x0007ae, "Greek_accentdieresis" },
  { 0x0007af, "Greek_horizbar" },
  { 0x0007b1, "Greek_alphaaccent" },
  { 0x0007b2, "Greek_epsilonaccent" },
  { 0x0007b3, "Greek_etaaccent" },
  { 0x0007b4, "Greek_iotaaccent" },
  { 0x0007b5, "Greek_iotadieresis" },
  { 0x0007b6, "Greek_iotaaccentdieresis" },
  { 0x0007b7, "Greek_omicronaccent" },
  { 0x0007b8, "Greek_upsilonaccent" },
  { 0x0007b9, "Greek_upsilondieresis" },
  { 0x0007ba, "Greek_upsilonaccentdieresis" },
  { 0x0007bb, "Greek_omegaaccent" },
  { 0x0007c1, "Greek_ALPHA" },
  { 0x0007c2, "Greek_BETA" },
  { 0x0007c3, "Greek_GAMMA" },
  { 0x0007c4, "Greek_DELTA" },
  { 0x0007c5, "Greek_EPSILON" },
  { 0x0007c6, "Greek_ZETA" },
  { 0x0007c7, "Greek_ETA" },
  { 0x0007c8, "Greek_THETA" },
  { 0x0007c9, "Greek_IOTA" },
  { 0x0007ca, "Greek_KAPPA" },
  { 0x0007cb, "Greek_LAMBDA" },
  { 0x0007cb, "Greek_LAMDA" },
  { 0x0007cc, "Greek_MU" },
  { 0x0007cd, "Greek_NU" },
  { 0x0007ce, "Greek_XI" },
  { 0x0007cf, "Greek_OMICRON" },
  { 0x0007d0, "Greek_PI" },
  { 0x0007d1, "Greek_RHO" },
  { 0x0007d2, "Greek_SIGMA" },
  { 0x0007d4, "Greek_TAU" },
  { 0x0007d5, "Greek_UPSILON" },
  { 0x0007d6, "Greek_PHI" },
  { 0x0007d7, "Greek_CHI" },
  { 0x0007d8, "Greek_PSI" },
  { 0x0007d9, "Greek_OMEGA" },
  { 0x0007e1, "Greek_alpha" },
  { 0x0007e2, "Greek_beta" },
  { 0x0007e3, "Greek_gamma" },
  { 0x0007e4, "Greek_delta" },
  { 0x0007e5, "Greek_epsilon" },
  { 0x0007e6, "Greek_zeta" },
  { 0x0007e7, "Greek_eta" },
  { 0x0007e8, "Greek_theta" },
  { 0x0007e9, "Greek_iota" },
  { 0x0007ea, "Greek_kappa" },
  { 0x0007eb, "Greek_lambda" },
  { 0x0007eb, "Greek_lamda" },
  { 0x0007ec, "Greek_mu" },
  { 0x0007ed, "Greek_nu" },
  { 0x0007ee, "Greek_xi" },
  { 0x0007ef, "Greek_omicron" },
  { 0x0007f0, "Greek_pi" },
  { 0x0007f1, "Greek_rho" },
  { 0x0007f2, "Greek_sigma" },
  { 0x0007f3, "Greek_finalsmallsigma" },
  { 0x0007f4, "Greek_tau" },
  { 0x0007f5, "Greek_upsilon" },
  { 0x0007f6, "Greek_phi" },
  { 0x0007f7, "Greek_chi" },
  { 0x0007f8, "Greek_psi" },
  { 0x0007f9, "Greek_omega" },
  { 0x0008a1, "leftradical" },
  { 0x0008a2, "topleftradical" },
  { 0x0008a3, "horizconnector" },
  { 0x0008a4, "topintegral" },
  { 0x0008a5, "botintegral" },
  { 0x0008a6, "vertconnector" },
  { 0x0008a7, "topleftsqbracket" },
  { 0x0008a8, "botleftsqbracket" },
  { 0x0008a9, "toprightsqbracket" },
  { 0x0008aa, "botrightsqbracket" },
  { 0x0008ab, "topleftparens" },
  { 0x0008ac, "botleftparens" },
  { 0x0008ad, "toprightparens" },
  { 0x0008ae, "botrightparens" },
  { 0x0008af, "leftmiddlecurlybrace" },
  { 0x0008b0, "rightmiddlecurlybrace" },
  { 0x0008b1, "topleftsummation" },
  { 0x0008b2, "botleftsummation" },
  { 0x0008b3, "topvertsummationconnector" },
  { 0x0008b4, "botvertsummationconnector" },
  { 0x0008b5, "toprightsummation" },
  { 0x0008b6, "botrightsummation" },
  { 0x0008b7, "rightmiddlesummation" },
  { 0x0008bc, "lessthanequal" },
  { 0x0008bd, "notequal" },
  { 0x0008be, "greaterthanequal" },
  { 0x0008bf, "integral" },
  { 0x0008c0, "therefore" },
  { 0x0008c1, "variation" },
  { 0x0008c2, "infinity" },
  { 0x0008c5, "nabla" },
  { 0x0008c8, "approximate" },
  { 0x0008c9, "similarequal" },
  { 0x0008cd, "ifonlyif" },
  { 0x0008ce, "implies" },
  { 0x0008cf, "identical" },
  { 0x0008d6, "radical" },
  { 0x0008da, "includedin" },
  { 0x0008db, "includes" },
  { 0x0008dc, "intersection" },
  { 0x0008dd, "union" },
  { 0x0008de, "logicaland" },
  { 0x0008df, "logicalor" },
  { 0x0008ef, "partialderivative" },
  { 0x0008f6, "function" },
  { 0x0008fb, "leftarrow" },
  { 0x0008fc, "uparrow" },
  { 0x0008fd, "rightarrow" },
  { 0x0008fe, "downarrow" },
  { 0x0009df, "blank" },
  { 0x0009e0, "soliddiamond" },
  { 0x0009e1, "checkerboard" },
  { 0x0009e2, "ht" },
  { 0x0009e3, "ff" },
  { 0x0009e4, "cr" },
  { 0x0009e5, "lf" },
  { 0x0009e8, "nl" },
  { 0x0009e9, "vt" },
  { 0x0009ea, "lowrightcorner" },
  { 0x0009eb, "uprightcorner" },
  { 0x0009ec, "upleftcorner" },
  { 0x0009ed, "lowleftcorner" },
  { 0x0009ee, "crossinglines" },
  { 0x0009ef, "horizlinescan1" },
  { 0x0009f0, "horizlinescan3" },
  { 0x0009f1, "horizlinescan5" },
  { 0x0009f2, "horizlinescan7" },
  { 0x0009f3, "horizlinescan9" },
  { 0x0009f4, "leftt" },
  { 0x0009f5, "rightt" },
  { 0x0009f6, "bott" },
  { 0x0009f7, "topt" },
  { 0x0009f8, "vertbar" },
  { 0x000aa1, "emspace" },
  { 0x000aa2, "enspace" },
  { 0x000aa3, "em3space" },
  { 0x000aa4, "em4space" },
  { 0x000aa5, "digitspace" },
  { 0x000aa6, "punctspace" },
  { 0x000aa7, "thinspace" },
  { 0x000aa8, "hairspace" },
  { 0x000aa9, "emdash" },
  { 0x000aaa, "endash" },
  { 0x000aac, "signifblank" },
  { 0x000aae, "ellipsis" },
  { 0x000aaf, "doubbaselinedot" },
  { 0x000ab0, "onethird" },
  { 0x000ab1, "twothirds" },
  { 0x000ab2, "onefifth" },
  { 0x000ab3, "twofifths" },
  { 0x000ab4, "threefifths" },
  { 0x000ab5, "fourfifths" },
  { 0x000ab6, "onesixth" },
  { 0x000ab7, "fivesixths" },
  { 0x000ab8, "careof" },
  { 0x000abb, "figdash" },
  { 0x000abc, "leftanglebracket" },
  { 0x000abd, "decimalpoint" },
  { 0x000abe, "rightanglebracket" },
  { 0x000abf, "marker" },
  { 0x000ac3, "oneeighth" },
  { 0x000ac4, "threeeighths" },
  { 0x000ac5, "fiveeighths" },
  { 0x000ac6, "seveneighths" },
  { 0x000ac9, "trademark" },
  { 0x000aca, "signaturemark" },
  { 0x000acb, "trademarkincircle" },
  { 0x000acc, "leftopentriangle" },
  { 0x000acd, "rightopentriangle" },
  { 0x000ace, "emopencircle" },
  { 0x000acf, "emopenrectangle" },
  { 0x000ad0, "leftsinglequotemark" },
  { 0x000ad1, "rightsinglequotemark" },
  { 0x000ad2, "leftdoublequotemark" },
  { 0x000ad3, "rightdoublequotemark" },
  { 0x000ad4, "prescription" },
  { 0x000ad6, "minutes" },
  { 0x000ad7, "seconds" },
  { 0x000ad9, "latincross" },
  { 0x000ada, "hexagram" },
  { 0x000adb, "filledrectbullet" },
  { 0x000adc, "filledlefttribullet" },
  { 0x000add, "filledrighttribullet" },
  { 0x000ade, "emfilledcircle" },
  { 0x000adf, "emfilledrect" },
  { 0x000ae0, "enopencircbullet" },
  { 0x000ae1, "enopensquarebullet" },
  { 0x000ae2, "openrectbullet" },
  { 0x000ae3, "opentribulletup" },
  { 0x000ae4, "opentribulletdown" },
  { 0x000ae5, "openstar" },
  { 0x000ae6, "enfilledcircbullet" },
  { 0x000ae7, "enfilledsqbullet" },
  { 0x000ae8, "filledtribulletup" },
  { 0x000ae9, "filledtribulletdown" },
  { 0x000aea, "leftpointer" },
  { 0x000aeb, "rightpointer" },
  { 0x000aec, "club" },
  { 0x000aed, "diamond" },
  { 0x000aee, "heart" },
  { 0x000af0, "maltesecross" },
  { 0x000af1, "dagger" },
  { 0x000af2, "doubledagger" },
  { 0x000af3, "checkmark" },
  { 0x000af4, "ballotcross" },
  { 0x000af5, "musicalsharp" },
  { 0x000af6, "musicalflat" },
  { 0x000af7, "malesymbol" },
  { 0x000af8, "femalesymbol" },
  { 0x000af9, "telephone" },
  { 0x000afa, "telephonerecorder" },
  { 0x000afb, "phonographcopyright" },
  { 0x000afc, "caret" },
  { 0x000afd, "singlelowquotemark" },
  { 0x000afe, "doublelowquotemark" },
  { 0x000aff, "cursor" },
  { 0x000ba3, "leftcaret" },
  { 0x000ba6, "rightcaret" },
  { 0x000ba8, "downcaret" },
  { 0x000ba9, "upcaret" },
  { 0x000bc0, "overbar" },
  { 0x000bc2, "downtack" },
  { 0x000bc3, "upshoe" },
  { 0x000bc4, "downstile" },
  { 0x000bc6, "underbar" },
  { 0x000bca, "jot" },
  { 0x000bcc, "quad" },
  { 0x000bce, "uptack" },
  { 0x000bcf, "circle" },
  { 0x000bd3, "upstile" },
  { 0x000bd6, "downshoe" },
  { 0x000bd8, "rightshoe" },
  { 0x000bda, "leftshoe" },
  { 0x000bdc, "lefttack" },
  { 0x000bfc, "righttack" },
  { 0x000cdf, "hebrew_doublelowline" },
  { 0x000ce0, "hebrew_aleph" },
  { 0x000ce1, "hebrew_bet" },
  { 0x000ce1, "hebrew_beth" },
  { 0x000ce2, "hebrew_gimel" },
  { 0x000ce2, "hebrew_gimmel" },
  { 0x000ce3, "hebrew_dalet" },
  { 0x000ce3, "hebrew_daleth" },
  { 0x000ce4, "hebrew_he" },
  { 0x000ce5, "hebrew_waw" },
  { 0x000ce6, "hebrew_zain" },
  { 0x000ce6, "hebrew_zayin" },
  { 0x000ce7, "hebrew_chet" },
  { 0x000ce7, "hebrew_het" },
  { 0x000ce8, "hebrew_tet" },
  { 0x000ce8, "hebrew_teth" },
  { 0x000ce9, "hebrew_yod" },
  { 0x000cea, "hebrew_finalkaph" },
  { 0x000ceb, "hebrew_kaph" },
  { 0x000cec, "hebrew_lamed" },
  { 0x000ced, "hebrew_finalmem" },
  { 0x000cee, "hebrew_mem" },
  { 0x000cef, "hebrew_finalnun" },
  { 0x000cf0, "hebrew_nun" },
  { 0x000cf1, "hebrew_samech" },
  { 0x000cf1, "hebrew_samekh" },
  { 0x000cf2, "hebrew_ayin" },
  { 0x000cf3, "hebrew_finalpe" },
  { 0x000cf4, "hebrew_pe" },
  { 0x000cf5, "hebrew_finalzade" },
  { 0x000cf5, "hebrew_finalzadi" },
  { 0x000cf6, "hebrew_zade" },
  { 0x000cf6, "hebrew_zadi" },
  { 0x000cf7, "hebrew_kuf" },
  { 0x000cf7, "hebrew_qoph" },
  { 0x000cf8, "hebrew_resh" },
  { 0x000cf9, "hebrew_shin" },
  { 0x000cfa, "hebrew_taf" },
  { 0x000cfa, "hebrew_taw" },
  { 0x000da1, "Thai_kokai" },
  { 0x000da2, "Thai_khokhai" },
  { 0x000da3, "Thai_khokhuat" },
  { 0x000da4, "Thai_khokhwai" },
  { 0x000da5, "Thai_khokhon" },
  { 0x000da6, "Thai_khorakhang" },
  { 0x000da7, "Thai_ngongu" },
  { 0x000da8, "Thai_chochan" },
  { 0x000da9, "Thai_choching" },
  { 0x000daa, "Thai_chochang" },
  { 0x000dab, "Thai_soso" },
  { 0x000dac, "Thai_chochoe" },
  { 0x000dad, "Thai_yoying" },
  { 0x000dae, "Thai_dochada" },
  { 0x000daf, "Thai_topatak" },
  { 0x000db0, "Thai_thothan" },
  { 0x000db1, "Thai_thonangmontho" },
  { 0x000db2, "Thai_thophuthao" },
  { 0x000db3, "Thai_nonen" },
  { 0x000db4, "Thai_dodek" },
  { 0x000db5, "Thai_totao" },
  { 0x000db6, "Thai_thothung" },
  { 0x000db7, "Thai_thothahan" },
  { 0x000db8, "Thai_thothong" },
  { 0x000db9, "Thai_nonu" },
  { 0x000dba, "Thai_bobaimai" },
  { 0x000dbb, "Thai_popla" },
  { 0x000dbc, "Thai_phophung" },
  { 0x000dbd, "Thai_fofa" },
  { 0x000dbe, "Thai_phophan" },
  { 0x000dbf, "Thai_fofan" },
  { 0x000dc0, "Thai_phosamphao" },
  { 0x000dc1, "Thai_moma" },
  { 0x000dc2, "Thai_yoyak" },
  { 0x000dc3, "Thai_rorua" },
  { 0x000dc4, "Thai_ru" },
  { 0x000dc5, "Thai_loling" },
  { 0x000dc6, "Thai_lu" },
  { 0x000dc7, "Thai_wowaen" },
  { 0x000dc8, "Thai_sosala" },
  { 0x000dc9, "Thai_sorusi" },
  { 0x000dca, "Thai_sosua" },
  { 0x000dcb, "Thai_hohip" },
  { 0x000dcc, "Thai_lochula" },
  { 0x000dcd, "Thai_oang" },
  { 0x000dce, "Thai_honokhuk" },
  { 0x000dcf, "Thai_paiyannoi" },
  { 0x000dd0, "Thai_saraa" },
  { 0x000dd1, "Thai_maihanakat" },
  { 0x000dd2, "Thai_saraaa" },
  { 0x000dd3, "Thai_saraam" },
  { 0x000dd4, "Thai_sarai" },
  { 0x000dd5, "Thai_saraii" },
  { 0x000dd6, "Thai_saraue" },
  { 0x000dd7, "Thai_sarauee" },
  { 0x000dd8, "Thai_sarau" },
  { 0x000dd9, "Thai_sarauu" },
  { 0x000dda, "Thai_phinthu" },
  { 0x000dde, "Thai_maihanakat_maitho" },
  { 0x000ddf, "Thai_baht" },
  { 0x000de0, "Thai_sarae" },
  { 0x000de1, "Thai_saraae" },
  { 0x000de2, "Thai_sarao" },
  { 0x000de3, "Thai_saraaimaimuan" },
  { 0x000de4, "Thai_saraaimaimalai" },
  { 0x000de5, "Thai_lakkhangyao" },
  { 0x000de6, "Thai_maiyamok" },
  { 0x000de7, "Thai_maitaikhu" },
  { 0x000de8, "Thai_maiek" },
  { 0x000de9, "Thai_maitho" },
  { 0x000dea, "Thai_maitri" },
  { 0x000deb, "Thai_maichattawa" },
  { 0x000dec, "Thai_thanthakhat" },
  { 0x000ded, "Thai_nikhahit" },
  { 0x000df0, "Thai_leksun" },
  { 0x000df1, "Thai_leknung" },
  { 0x000df2, "Thai_leksong" },
  { 0x000df3, "Thai_leksam" },
  { 0x000df4, "Thai_leksi" },
  { 0x000df5, "Thai_lekha" },
  { 0x000df6, "Thai_lekhok" },
  { 0x000df7, "Thai_lekchet" },
  { 0x000df8, "Thai_lekpaet" },
  { 0x000df9, "Thai_lekkao" },
  { 0x000ea1, "Hangul_Kiyeog" },
  { 0x000ea2, "Hangul_SsangKiyeog" },
  { 0x000ea3, "Hangul_KiyeogSios" },
  { 0x000ea4, "Hangul_Nieun" },
  { 0x000ea5, "Hangul_NieunJieuj" },
  { 0x000ea6, "Hangul_NieunHieuh" },
  { 0x000ea7, "Hangul_Dikeud" },
  { 0x000ea8, "Hangul_SsangDikeud" },
  { 0x000ea9, "Hangul_Rieul" },
  { 0x000eaa, "Hangul_RieulKiyeog" },
  { 0x000eab, "Hangul_RieulMieum" },
  { 0x000eac, "Hangul_RieulPieub" },
  { 0x000ead, "Hangul_RieulSios" },
  { 0x000eae, "Hangul_RieulTieut" },
  { 0x000eaf, "Hangul_RieulPhieuf" },
  { 0x000eb0, "Hangul_RieulHieuh" },
  { 0x000eb1, "Hangul_Mieum" },
  { 0x000eb2, "Hangul_Pieub" },
  { 0x000eb3, "Hangul_SsangPieub" },
  { 0x000eb4, "Hangul_PieubSios" },
  { 0x000eb5, "Hangul_Sios" },
  { 0x000eb6, "Hangul_SsangSios" },
  { 0x000eb7, "Hangul_Ieung" },
  { 0x000eb8, "Hangul_Jieuj" },
  { 0x000eb9, "Hangul_SsangJieuj" },
  { 0x000eba, "Hangul_Cieuc" },
  { 0x000ebb, "Hangul_Khieuq" },
  { 0x000ebc, "Hangul_Tieut" },
  { 0x000ebd, "Hangul_Phieuf" },
  { 0x000ebe, "Hangul_Hieuh" },
  { 0x000ebf, "Hangul_A" },
  { 0x000ec0, "Hangul_AE" },
  { 0x000ec1, "Hangul_YA" },
  { 0x000ec2, "Hangul_YAE" },
  { 0x000ec3, "Hangul_EO" },
  { 0x000ec4, "Hangul_E" },
  { 0x000ec5, "Hangul_YEO" },
  { 0x000ec6, "Hangul_YE" },
  { 0x000ec7, "Hangul_O" },
  { 0x000ec8, "Hangul_WA" },
  { 0x000ec9, "Hangul_WAE" },
  { 0x000eca, "Hangul_OE" },
  { 0x000ecb, "Hangul_YO" },
  { 0x000ecc, "Hangul_U" },
  { 0x000ecd, "Hangul_WEO" },
  { 0x000ece, "Hangul_WE" },
  { 0x000ecf, "Hangul_WI" },
  { 0x000ed0, "Hangul_YU" },
  { 0x000ed1, "Hangul_EU" },
  { 0x000ed2, "Hangul_YI" },
  { 0x000ed3, "Hangul_I" },
  { 0x000ed4, "Hangul_J_Kiyeog" },
  { 0x000ed5, "Hangul_J_SsangKiyeog" },
  { 0x000ed6, "Hangul_J_KiyeogSios" },
  { 0x000ed7, "Hangul_J_Nieun" },
  { 0x000ed8, "Hangul_J_NieunJieuj" },
  { 0x000ed9, "Hangul_J_NieunHieuh" },
  { 0x000eda, "Hangul_J_Dikeud" },
  { 0x000edb, "Hangul_J_Rieul" },
  { 0x000edc, "Hangul_J_RieulKiyeog" },
  { 0x000edd, "Hangul_J_RieulMieum" },
  { 0x000ede, "Hangul_J_RieulPieub" },
  { 0x000edf, "Hangul_J_RieulSios" },
  { 0x000ee0, "Hangul_J_RieulTieut" },
  { 0x000ee1, "Hangul_J_RieulPhieuf" },
  { 0x000ee2, "Hangul_J_RieulHieuh" },
  { 0x000ee3, "Hangul_J_Mieum" },
  { 0x000ee4, "Hangul_J_Pieub" },
  { 0x000ee5, "Hangul_J_PieubSios" },
  { 0x000ee6, "Hangul_J_Sios" },
  { 0x000ee7, "Hangul_J_SsangSios" },
  { 0x000ee8, "Hangul_J_Ieung" },
  { 0x000ee9, "Hangul_J_Jieuj" },
  { 0x000eea, "Hangul_J_Cieuc" },
  { 0x000eeb, "Hangul_J_Khieuq" },
  { 0x000eec, "Hangul_J_Tieut" },
  { 0x000eed, "Hangul_J_Phieuf" },
  { 0x000eee, "Hangul_J_Hieuh" },
  { 0x000eef, "Hangul_RieulYeorinHieuh" },
  { 0x000ef0, "Hangul_SunkyeongeumMieum" },
  { 0x000ef1, "Hangul_SunkyeongeumPieub" },
  { 0x000ef2, "Hangul_PanSios" },
  { 0x000ef3, "Hangul_KkogjiDalrinIeung" },
  { 0x000ef4, "Hangul_SunkyeongeumPhieuf" },
  { 0x000ef5, "Hangul_YeorinHieuh" },
  { 0x000ef6, "Hangul_AraeA" },
  { 0x000ef7, "Hangul_AraeAE" },
  { 0x000ef8, "Hangul_J_PanSios" },
  { 0x000ef9, "Hangul_J_KkogjiDalrinIeung" },
  { 0x000efa, "Hangul_J_YeorinHieuh" },
  { 0x000eff, "Korean_Won" },
  { 0x0013bc, "OE" },
  { 0x0013bd, "oe" },
  { 0x0013be, "Ydiaeresis" },
  { 0x0020a0, "EcuSign" },
  { 0x0020a1, "ColonSign" },
  { 0x0020a2, "CruzeiroSign" },
  { 0x0020a3, "FFrancSign" },
  { 0x0020a4, "LiraSign" },
  { 0x0020a5, "MillSign" },
  { 0x0020a6, "NairaSign" },
  { 0x0020a7, "PesetaSign" },
  { 0x0020a8, "RupeeSign" },
  { 0x0020a9, "WonSign" },
  { 0x0020aa, "NewSheqelSign" },
  { 0x0020ab, "DongSign" },
  { 0x0020ac, "EuroSign" },
  { 0x00fd01, "3270_Duplicate" },
  { 0x00fd02, "3270_FieldMark" },
  { 0x00fd03, "3270_Right2" },
  { 0x00fd04, "3270_Left2" },
  { 0x00fd05, "3270_BackTab" },
  { 0x00fd06, "3270_EraseEOF" },
  { 0x00fd07, "3270_EraseInput" },
  { 0x00fd08, "3270_Reset" },
  { 0x00fd09, "3270_Quit" },
  { 0x00fd0a, "3270_PA1" },
  { 0x00fd0b, "3270_PA2" },
  { 0x00fd0c, "3270_PA3" },
  { 0x00fd0d, "3270_Test" },
  { 0x00fd0e, "3270_Attn" },
  { 0x00fd0f, "3270_CursorBlink" },
  { 0x00fd10, "3270_AltCursor" },
  { 0x00fd11, "3270_KeyClick" },
  { 0x00fd12, "3270_Jump" },
  { 0x00fd13, "3270_Ident" },
  { 0x00fd14, "3270_Rule" },
  { 0x00fd15, "3270_Copy" },
  { 0x00fd16, "3270_Play" },
  { 0x00fd17, "3270_Setup" },
  { 0x00fd18, "3270_Record" },
  { 0x00fd19, "3270_ChangeScreen" },
  { 0x00fd1a, "3270_DeleteWord" },
  { 0x00fd1b, "3270_ExSelect" },
  { 0x00fd1c, "3270_CursorSelect" },
  { 0x00fd1d, "3270_PrintScreen" },
  { 0x00fd1e, "3270_Enter" },
  { 0x00fe01, "ISO_Lock" },
  { 0x00fe02, "ISO_Level2_Latch" },
  { 0x00fe03, "ISO_Level3_Shift" },
  { 0x00fe04, "ISO_Level3_Latch" },
  { 0x00fe05, "ISO_Level3_Lock" },
  { 0x00fe06, "ISO_Group_Latch" },
  { 0x00fe07, "ISO_Group_Lock" },
  { 0x00fe08, "ISO_Next_Group" },
  { 0x00fe09, "ISO_Next_Group_Lock" },
  { 0x00fe0a, "ISO_Prev_Group" },
  { 0x00fe0b, "ISO_Prev_Group_Lock" },
  { 0x00fe0c, "ISO_First_Group" },
  { 0x00fe0d, "ISO_First_Group_Lock" },
  { 0x00fe0e, "ISO_Last_Group" },
  { 0x00fe0f, "ISO_Last_Group_Lock" },
  { 0x00fe20, "ISO_Left_Tab" },
  { 0x00fe21, "ISO_Move_Line_Up" },
  { 0x00fe22, "ISO_Move_Line_Down" },
  { 0x00fe23, "ISO_Partial_Line_Up" },
  { 0x00fe24, "ISO_Partial_Line_Down" },
  { 0x00fe25, "ISO_Partial_Space_Left" },
  { 0x00fe26, "ISO_Partial_Space_Right" },
  { 0x00fe27, "ISO_Set_Margin_Left" },
  { 0x00fe28, "ISO_Set_Margin_Right" },
  { 0x00fe29, "ISO_Release_Margin_Left" },
  { 0x00fe2a, "ISO_Release_Margin_Right" },
  { 0x00fe2b, "ISO_Release_Both_Margins" },
  { 0x00fe2c, "ISO_Fast_Cursor_Left" },
  { 0x00fe2d, "ISO_Fast_Cursor_Right" },
  { 0x00fe2e, "ISO_Fast_Cursor_Up" },
  { 0x00fe2f, "ISO_Fast_Cursor_Down" },
  { 0x00fe30, "ISO_Continuous_Underline" },
  { 0x00fe31, "ISO_Discontinuous_Underline" },
  { 0x00fe32, "ISO_Emphasize" },
  { 0x00fe33, "ISO_Center_Object" },
  { 0x00fe34, "ISO_Enter" },
  { 0x00fe50, "dead_grave" },
  { 0x00fe51, "dead_acute" },
  { 0x00fe52, "dead_circumflex" },
  { 0x00fe53, "dead_tilde" },
  { 0x00fe54, "dead_macron" },
  { 0x00fe55, "dead_breve" },
  { 0x00fe56, "dead_abovedot" },
  { 0x00fe57, "dead_diaeresis" },
  { 0x00fe58, "dead_abovering" },
  { 0x00fe59, "dead_doubleacute" },
  { 0x00fe5a, "dead_caron" },
  { 0x00fe5b, "dead_cedilla" },
  { 0x00fe5c, "dead_ogonek" },
  { 0x00fe5d, "dead_iota" },
  { 0x00fe5e, "dead_voiced_sound" },
  { 0x00fe5f, "dead_semivoiced_sound" },
  { 0x00fe60, "dead_belowdot" },
  { 0x00fe70, "AccessX_Enable" },
  { 0x00fe71, "AccessX_Feedback_Enable" },
  { 0x00fe72, "RepeatKeys_Enable" },
  { 0x00fe73, "SlowKeys_Enable" },
  { 0x00fe74, "BounceKeys_Enable" },
  { 0x00fe75, "StickyKeys_Enable" },
  { 0x00fe76, "MouseKeys_Enable" },
  { 0x00fe77, "MouseKeys_Accel_Enable" },
  { 0x00fe78, "Overlay1_Enable" },
  { 0x00fe79, "Overlay2_Enable" },
  { 0x00fe7a, "AudibleBell_Enable" },
  { 0x00fed0, "First_Virtual_Screen" },
  { 0x00fed1, "Prev_Virtual_Screen" },
  { 0x00fed2, "Next_Virtual_Screen" },
  { 0x00fed4, "Last_Virtual_Screen" },
  { 0x00fed5, "Terminate_Server" },
  { 0x00fee0, "Pointer_Left" },
  { 0x00fee1, "Pointer_Right" },
  { 0x00fee2, "Pointer_Up" },
  { 0x00fee3, "Pointer_Down" },
  { 0x00fee4, "Pointer_UpLeft" },
  { 0x00fee5, "Pointer_UpRight" },
  { 0x00fee6, "Pointer_DownLeft" },
  { 0x00fee7, "Pointer_DownRight" },
  { 0x00fee8, "Pointer_Button_Dflt" },
  { 0x00fee9, "Pointer_Button1" },
  { 0x00feea, "Pointer_Button2" },
  { 0x00feeb, "Pointer_Button3" },
  { 0x00feec, "Pointer_Button4" },
  { 0x00feed, "Pointer_Button5" },
  { 0x00feee, "Pointer_DblClick_Dflt" },
  { 0x00feef, "Pointer_DblClick1" },
  { 0x00fef0, "Pointer_DblClick2" },
  { 0x00fef1, "Pointer_DblClick3" },
  { 0x00fef2, "Pointer_DblClick4" },
  { 0x00fef3, "Pointer_DblClick5" },
  { 0x00fef4, "Pointer_Drag_Dflt" },
  { 0x00fef5, "Pointer_Drag1" },
  { 0x00fef6, "Pointer_Drag2" },
  { 0x00fef7, "Pointer_Drag3" },
  { 0x00fef8, "Pointer_Drag4" },
  { 0x00fef9, "Pointer_EnableKeys" },
  { 0x00fefa, "Pointer_Accelerate" },
  { 0x00fefb, "Pointer_DfltBtnNext" },
  { 0x00fefc, "Pointer_DfltBtnPrev" },
  { 0x00fefd, "Pointer_Drag5" },
  { 0x00ff08, "BackSpace" },
  { 0x00ff09, "Tab" },
  { 0x00ff0a, "Linefeed" },
  { 0x00ff0b, "Clear" },
  { 0x00ff0d, "Return" },
  { 0x00ff13, "Pause" },
  { 0x00ff14, "Scroll_Lock" },
  { 0x00ff15, "Sys_Req" },
  { 0x00ff1b, "Escape" },
  { 0x00ff20, "Multi_key" },
  { 0x00ff21, "Kanji" },
  { 0x00ff22, "Muhenkan" },
  { 0x00ff23, "Henkan" },
  { 0x00ff23, "Henkan_Mode" },
  { 0x00ff24, "Romaji" },
  { 0x00ff25, "Hiragana" },
  { 0x00ff26, "Katakana" },
  { 0x00ff27, "Hiragana_Katakana" },
  { 0x00ff28, "Zenkaku" },
  { 0x00ff29, "Hankaku" },
  { 0x00ff2a, "Zenkaku_Hankaku" },
  { 0x00ff2b, "Touroku" },
  { 0x00ff2c, "Massyo" },
  { 0x00ff2d, "Kana_Lock" },
  { 0x00ff2e, "Kana_Shift" },
  { 0x00ff2f, "Eisu_Shift" },
  { 0x00ff30, "Eisu_toggle" },
  { 0x00ff31, "Hangul" },
  { 0x00ff32, "Hangul_Start" },
  { 0x00ff33, "Hangul_End" },
  { 0x00ff34, "Hangul_Hanja" },
  { 0x00ff35, "Hangul_Jamo" },
  { 0x00ff36, "Hangul_Romaja" },
  { 0x00ff37, "Codeinput" },
  { 0x00ff38, "Hangul_Jeonja" },
  { 0x00ff39, "Hangul_Banja" },
  { 0x00ff3a, "Hangul_PreHanja" },
  { 0x00ff3b, "Hangul_PostHanja" },
  { 0x00ff3c, "SingleCandidate" },
  { 0x00ff3d, "MultipleCandidate" },
  { 0x00ff3e, "PreviousCandidate" },
  { 0x00ff3f, "Hangul_Special" },
  { 0x00ff50, "Home" },
  { 0x00ff51, "Left" },
  { 0x00ff52, "Up" },
  { 0x00ff53, "Right" },
  { 0x00ff54, "Down" },
  { 0x00ff55, "Page_Up" },
  { 0x00ff55, "Prior" },
  { 0x00ff56, "Page_Down" },
  { 0x00ff56, "Next" },
  { 0x00ff57, "End" },
  { 0x00ff58, "Begin" },
  { 0x00ff60, "Select" },
  { 0x00ff61, "Print" },
  { 0x00ff62, "Execute" },
  { 0x00ff63, "Insert" },
  { 0x00ff65, "Undo" },
  { 0x00ff66, "Redo" },
  { 0x00ff67, "Menu" },
  { 0x00ff68, "Find" },
  { 0x00ff69, "Cancel" },
  { 0x00ff6a, "Help" },
  { 0x00ff6b, "Break" },
  { 0x00ff7e, "Arabic_switch" },
  { 0x00ff7e, "Greek_switch" },
  { 0x00ff7e, "Hangul_switch" },
  { 0x00ff7e, "Hebrew_switch" },
  { 0x00ff7e, "ISO_Group_Shift" },
  { 0x00ff7e, "Mode_switch" },
  { 0x00ff7e, "kana_switch" },
  { 0x00ff7e, "script_switch" },
  { 0x00ff7f, "Num_Lock" },
  { 0x00ff80, "KP_Space" },
  { 0x00ff89, "KP_Tab" },
  { 0x00ff8d, "KP_Enter" },
  { 0x00ff91, "KP_F1" },
  { 0x00ff92, "KP_F2" },
  { 0x00ff93, "KP_F3" },
  { 0x00ff94, "KP_F4" },
  { 0x00ff95, "KP_Home" },
  { 0x00ff96, "KP_Left" },
  { 0x00ff97, "KP_Up" },
  { 0x00ff98, "KP_Right" },
  { 0x00ff99, "KP_Down" },
  { 0x00ff9a, "KP_Page_Up" },
  { 0x00ff9a, "KP_Prior" },
  { 0x00ff9b, "KP_Page_Down" },
  { 0x00ff9b, "KP_Next" },
  { 0x00ff9c, "KP_End" },
  { 0x00ff9d, "KP_Begin" },
  { 0x00ff9e, "KP_Insert" },
  { 0x00ff9f, "KP_Delete" },
  { 0x00ffaa, "KP_Multiply" },
  { 0x00ffab, "KP_Add" },
  { 0x00ffac, "KP_Separator" },
  { 0x00ffad, "KP_Subtract" },
  { 0x00ffae, "KP_Decimal" },
  { 0x00ffaf, "KP_Divide" },
  { 0x00ffb0, "KP_0" },
  { 0x00ffb1, "KP_1" },
  { 0x00ffb2, "KP_2" },
  { 0x00ffb3, "KP_3" },
  { 0x00ffb4, "KP_4" },
  { 0x00ffb5, "KP_5" },
  { 0x00ffb6, "KP_6" },
  { 0x00ffb7, "KP_7" },
  { 0x00ffb8, "KP_8" },
  { 0x00ffb9, "KP_9" },
  { 0x00ffbd, "KP_Equal" },
  { 0x00ffbe, "F1" },
  { 0x00ffbf, "F2" },
  { 0x00ffc0, "F3" },
  { 0x00ffc1, "F4" },
  { 0x00ffc2, "F5" },
  { 0x00ffc3, "F6" },
  { 0x00ffc4, "F7" },
  { 0x00ffc5, "F8" },
  { 0x00ffc6, "F9" },
  { 0x00ffc7, "F10" },
  { 0x00ffc8, "F11" },
  { 0x00ffc9, "F12" },
  { 0x00ffca, "F13" },
  { 0x00ffcb, "F14" },
  { 0x00ffcc, "F15" },
  { 0x00ffcd, "F16" },
  { 0x00ffce, "F17" },
  { 0x00ffcf, "F18" },
  { 0x00ffd0, "F19" },
  { 0x00ffd1, "F20" },
  { 0x00ffd2, "F21" },
  { 0x00ffd3, "F22" },
  { 0x00ffd4, "F23" },
  { 0x00ffd5, "F24" },
  { 0x00ffd6, "F25" },
  { 0x00ffd7, "F26" },
  { 0x00ffd8, "F27" },
  { 0x00ffd9, "F28" },
  { 0x00ffda, "F29" },
  { 0x00ffdb, "F30" },
  { 0x00ffdc, "F31" },
  { 0x00ffdd, "F32" },
  { 0x00ffde, "F33" },
  { 0x00ffdf, "F34" },
  { 0x00ffe0, "F35" },
  { 0x00ffe1, "Shift_L" },
  { 0x00ffe2, "Shift_R" },
  { 0x00ffe3, "Control_L" },
  { 0x00ffe4, "Control_R" },
  { 0x00ffe5, "Caps_Lock" },
  { 0x00ffe6, "Shift_Lock" },
  { 0x00ffe7, "Meta_L" },
  { 0x00ffe8, "Meta_R" },
  { 0x00ffe9, "Alt_L" },
  { 0x00ffea, "Alt_R" },
  { 0x00ffeb, "Super_L" },
  { 0x00ffec, "Super_R" },
  { 0x00ffed, "Hyper_L" },
  { 0x00ffee, "Hyper_R" },
  { 0x00ffff, "Delete" },
  { 0xffffff, "VoidSymbol" },
};

#define GDK_NUM_KEYS (sizeof (gdk_keys_by_keyval) / sizeof (gdk_keys_by_keyval[0]))

static struct gdk_key *gdk_keys_by_name = NULL;

static int
gdk_keys_keyval_compare (const void *pkey, const void *pbase)
{
  return (*(int *) pkey) - ((struct gdk_key *) pbase)->keyval;
}

gchar*
gdk_keyval_name (guint keyval)
{
  static gchar buf[100];
  struct gdk_key *found;

  /* Check for directly encoded 24-bit UCS characters: */
  if ((keyval & 0xff000000) == 0x01000000)
    {
      sprintf (buf, "U+%.04X", (keyval & 0x00ffffff));
      return buf;
    }

  found = bsearch (&keyval, gdk_keys_by_keyval,
		   GDK_NUM_KEYS, sizeof (struct gdk_key),
		   gdk_keys_keyval_compare);

  if (found != NULL)
    {
      while ((found > gdk_keys_by_keyval) &&
             ((found - 1)->keyval == keyval))
        found--;
	    
      return (gchar *) found->name;
    }
  else
    {
      sprintf (buf, "%#x", keyval);
      return buf;
    }
}

static int 
gdk_key_compare_by_name (const void *a, const void *b)
{
  return strcmp (((const struct gdk_key *) a)->name, ((const struct gdk_key *) b)->name);
}

static int
gdk_keys_name_compare (const void *pkey, const void *pbase)
{
  return strcmp ((const char *) pkey, ((const struct gdk_key *) pbase)->name);
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  struct gdk_key *found;

  g_return_val_if_fail (keyval_name != NULL, 0);
  
  if (gdk_keys_by_name == NULL)
    {
      gdk_keys_by_name = g_new (struct gdk_key, GDK_NUM_KEYS);

      memcpy (gdk_keys_by_name, gdk_keys_by_keyval,
	      GDK_NUM_KEYS * sizeof (struct gdk_key));

      qsort (gdk_keys_by_name, GDK_NUM_KEYS, sizeof (struct gdk_key),
	     gdk_key_compare_by_name);
    }

  found = bsearch (keyval_name, gdk_keys_by_name,
		   GDK_NUM_KEYS, sizeof (struct gdk_key),
		   gdk_keys_name_compare);
  if (found != NULL)
    return found->keyval;
  else
    return GDK_VoidSymbol;
}
