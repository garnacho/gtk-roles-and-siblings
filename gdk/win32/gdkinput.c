/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1999 Tor Lillqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "gdk.h"
#include "gdkx.h"
#include "gdkprivate.h"

#include "gdkinput.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* If USE_SYSCONTEXT is on, we open the Wintab device (hmm, what if
 * there are several?) as a system pointing device, i.e. it controls
 * the normal Windows cursor. This seems much more natural.
 */
#define USE_SYSCONTEXT 1	/* The code for the other choice is not
				 * good at all.
				 */

#define TWOPI (2.*M_PI)

#define PING() g_print("%s: %d\n",__FILE__,__LINE__)

/* Forward declarations */

static gint gdk_input_win32_set_mode (guint32      deviceid,
				      GdkInputMode mode);
static void gdk_input_win32_get_pointer (GdkWindow       *window,
					 guint32	    deviceid,
					 gdouble         *x,
					 gdouble         *y,
					 gdouble         *pressure,
					 gdouble         *xtilt,
					 gdouble         *ytilt,
					 GdkModifierType *mask);
static void gdk_input_none_get_pointer (GdkWindow       *window,
					guint32		 deviceid,
					gdouble         *x,
					gdouble         *y,
					gdouble         *pressure,
					gdouble         *xtilt,
					gdouble         *ytilt,
					GdkModifierType *mask);
static gint gdk_input_win32_grab_pointer (GdkWindow *     window,
					  gint            owner_events,
					  GdkEventMask    event_mask,
					  GdkWindow      *confine_to,
					  guint32         time);
static void gdk_input_win32_ungrab_pointer (guint32 time);
static void gdk_input_win32_configure_event (GdkEventConfigure *event, 
					     GdkWindow         *window);
static void gdk_input_win32_enter_event (GdkEventCrossing  *xevent, 
					 GdkWindow         *window);
static gint gdk_input_win32_other_event (GdkEvent  *event, 
					 MSG       *xevent);
static gint gdk_input_win32_enable_window (GdkWindow        *window,
					   GdkDevicePrivate *gdkdev);
static gint gdk_input_win32_disable_window (GdkWindow        *window,
					    GdkDevicePrivate *gdkdev);

static GdkInputWindow *gdk_input_window_find (GdkWindow *window);
static GdkInputWindow *gdk_input_window_find_within (GdkWindow *window);
static GdkDevicePrivate *gdk_input_find_device (guint32 deviceid);
static GdkDevicePrivate *gdk_input_find_dev_from_ctx (HCTX hctx,
						      UINT id);

/* Local variables */

static GList     *gdk_input_devices;
static GList     *gdk_input_windows;
static GList     *wintab_contexts;

static gint gdk_input_root_width;
static gint gdk_input_root_height;

static GdkWindow *wintab_window;

static guint32 last_moved_cursor_id;

static GdkAxisUse gdk_input_core_axes[] = { GDK_AXIS_X, GDK_AXIS_Y };

static GdkDeviceInfo gdk_input_core_info =
{
  GDK_CORE_POINTER,
  "Core Pointer",
  GDK_SOURCE_MOUSE,
  GDK_MODE_SCREEN,
  TRUE,
  2,
  gdk_input_core_axes
};

/* Global variables  */

GdkInputVTable    gdk_input_vtable;
gint              gdk_input_ignore_core;
gint		  gdk_input_ignore_wintab = FALSE;

#if 0

static void
print_lc(LOGCONTEXT *lc)
{
  g_print ("lcName = %s\n", lc->lcName);
  g_print ("lcOptions =");
  if (lc->lcOptions & CXO_SYSTEM) g_print (" CXO_SYSTEM");
  if (lc->lcOptions & CXO_PEN) g_print (" CXO_PEN");
  if (lc->lcOptions & CXO_MESSAGES) g_print (" CXO_MESSAGES");
  if (lc->lcOptions & CXO_MARGIN) g_print (" CXO_MARGIN");
  if (lc->lcOptions & CXO_MGNINSIDE) g_print (" CXO_MGNINSIDE");
  if (lc->lcOptions & CXO_CSRMESSAGES) g_print (" CXO_CSRMESSAGES");
  if (lc->lcOptions & CXO_CSRMESSAGES) g_print (" CXO_CSRMESSAGES");
  g_print ("\n");
  g_print ("lcStatus =");
  if (lc->lcStatus & CXS_DISABLED) g_print (" CXS_DISABLED");
  if (lc->lcStatus & CXS_OBSCURED) g_print (" CXS_OBSCURED");
  if (lc->lcStatus & CXS_ONTOP) g_print (" CXS_ONTOP");
  g_print ("\n");
  g_print ("lcLocks =");
  if (lc->lcLocks & CXL_INSIZE) g_print (" CXL_INSIZE");
  if (lc->lcLocks & CXL_INASPECT) g_print (" CXL_INASPECT");
  if (lc->lcLocks & CXL_SENSITIVITY) g_print (" CXL_SENSITIVITY");
  if (lc->lcLocks & CXL_MARGIN) g_print (" CXL_MARGIN");
  g_print ("\n");
  g_print ("lcMsgBase = %#x, lcDevice = %#x, lcPktRate = %d\n",
	  lc->lcMsgBase, lc->lcDevice, lc->lcPktRate);
  g_print ("lcPktData =");
  if (lc->lcPktData & PK_CONTEXT) g_print (" PK_CONTEXT");
  if (lc->lcPktData & PK_STATUS) g_print (" PK_STATUS");
  if (lc->lcPktData & PK_TIME) g_print (" PK_TIME");
  if (lc->lcPktData & PK_CHANGED) g_print (" PK_CHANGED");
  if (lc->lcPktData & PK_SERIAL_NUMBER) g_print (" PK_SERIAL_NUMBER");
  if (lc->lcPktData & PK_CURSOR) g_print (" PK_CURSOR");
  if (lc->lcPktData & PK_BUTTONS) g_print (" PK_BUTTONS");
  if (lc->lcPktData & PK_X) g_print (" PK_X");
  if (lc->lcPktData & PK_Y) g_print (" PK_Y");
  if (lc->lcPktData & PK_Z) g_print (" PK_Z");
  if (lc->lcPktData & PK_NORMAL_PRESSURE) g_print (" PK_NORMAL_PRESSURE");
  if (lc->lcPktData & PK_TANGENT_PRESSURE) g_print (" PK_TANGENT_PRESSURE");
  if (lc->lcPktData & PK_ORIENTATION) g_print (" PK_ORIENTATION");
  if (lc->lcPktData & PK_ROTATION) g_print (" PK_ROTATION");
  g_print ("\n");
  g_print ("lcPktMode =");
  if (lc->lcPktMode & PK_CONTEXT) g_print (" PK_CONTEXT");
  if (lc->lcPktMode & PK_STATUS) g_print (" PK_STATUS");
  if (lc->lcPktMode & PK_TIME) g_print (" PK_TIME");
  if (lc->lcPktMode & PK_CHANGED) g_print (" PK_CHANGED");
  if (lc->lcPktMode & PK_SERIAL_NUMBER) g_print (" PK_SERIAL_NUMBER");
  if (lc->lcPktMode & PK_CURSOR) g_print (" PK_CURSOR");
  if (lc->lcPktMode & PK_BUTTONS) g_print (" PK_BUTTONS");
  if (lc->lcPktMode & PK_X) g_print (" PK_X");
  if (lc->lcPktMode & PK_Y) g_print (" PK_Y");
  if (lc->lcPktMode & PK_Z) g_print (" PK_Z");
  if (lc->lcPktMode & PK_NORMAL_PRESSURE) g_print (" PK_NORMAL_PRESSURE");
  if (lc->lcPktMode & PK_TANGENT_PRESSURE) g_print (" PK_TANGENT_PRESSURE");
  if (lc->lcPktMode & PK_ORIENTATION) g_print (" PK_ORIENTATION");
  if (lc->lcPktMode & PK_ROTATION) g_print (" PK_ROTATION");
  g_print ("\n");
  g_print ("lcMoveMask =");
  if (lc->lcMoveMask & PK_CONTEXT) g_print (" PK_CONTEXT");
  if (lc->lcMoveMask & PK_STATUS) g_print (" PK_STATUS");
  if (lc->lcMoveMask & PK_TIME) g_print (" PK_TIME");
  if (lc->lcMoveMask & PK_CHANGED) g_print (" PK_CHANGED");
  if (lc->lcMoveMask & PK_SERIAL_NUMBER) g_print (" PK_SERIAL_NUMBER");
  if (lc->lcMoveMask & PK_CURSOR) g_print (" PK_CURSOR");
  if (lc->lcMoveMask & PK_BUTTONS) g_print (" PK_BUTTONS");
  if (lc->lcMoveMask & PK_X) g_print (" PK_X");
  if (lc->lcMoveMask & PK_Y) g_print (" PK_Y");
  if (lc->lcMoveMask & PK_Z) g_print (" PK_Z");
  if (lc->lcMoveMask & PK_NORMAL_PRESSURE) g_print (" PK_NORMAL_PRESSURE");
  if (lc->lcMoveMask & PK_TANGENT_PRESSURE) g_print (" PK_TANGENT_PRESSURE");
  if (lc->lcMoveMask & PK_ORIENTATION) g_print (" PK_ORIENTATION");
  if (lc->lcMoveMask & PK_ROTATION) g_print (" PK_ROTATION");
  g_print ("\n");
  g_print ("lcBtnDnMask = %#x, lcBtnUpMask = %#x\n",
	  lc->lcBtnDnMask, lc->lcBtnUpMask);
  g_print ("lcInOrgX = %d, lcInOrgY = %d, lcInOrgZ = %d\n",
	  lc->lcInOrgX, lc->lcInOrgY, lc->lcInOrgZ);
  g_print ("lcInExtX = %d, lcInExtY = %d, lcInExtZ = %d\n",
	  lc->lcInExtX, lc->lcInExtY, lc->lcInExtZ);
  g_print ("lcOutOrgX = %d, lcOutOrgY = %d, lcOutOrgZ = %d\n",
	  lc->lcOutOrgX, lc->lcOutOrgY, lc->lcOutOrgZ);
  g_print ("lcOutExtX = %d, lcOutExtY = %d, lcOutExtZ = %d\n",
	  lc->lcOutExtX, lc->lcOutExtY, lc->lcOutExtZ);
  g_print ("lcSensX = %g, lcSensY = %g, lcSensZ = %g\n",
	  lc->lcSensX / 65536., lc->lcSensY / 65536., lc->lcSensZ / 65536.);
  g_print ("lcSysMode = %d\n", lc->lcSysMode);
  g_print ("lcSysOrgX = %d, lcSysOrgY = %d\n",
	  lc->lcSysOrgX, lc->lcSysOrgY);
  g_print ("lcSysExtX = %d, lcSysExtY = %d\n",
	  lc->lcSysExtX, lc->lcSysExtY);
  g_print ("lcSysSensX = %g, lcSysSensY = %g\n",
	  lc->lcSysSensX / 65536., lc->lcSysSensY / 65536.);
}

#endif

void 
gdk_input_init (void)
{
  GdkDevicePrivate *gdkdev;
  GdkWindowPrivate *window_private;
  GdkWindowAttr wa;
  WORD specversion;
  LOGCONTEXT defcontext;
  HCTX *hctx;
  UINT ndevices, ncursors, ncsrtypes, firstcsr, hardware;
  AXIS axis_x, axis_y, axis_npressure, axis_orientation[3];
  int i, j, k;
  char devname[100], csrname[100];
  guint32 deviceid_counter = 0;

  gdk_input_devices = NULL;
  wintab_contexts = NULL;

  if (!gdk_input_ignore_wintab &&
      WTInfo (0, 0, NULL))
    {
      WTInfo (WTI_INTERFACE, IFC_SPECVERSION, &specversion);

#if USE_SYSCONTEXT
      WTInfo (WTI_DEFSYSCTX, 0, &defcontext);
#else
      WTInfo (WTI_DEFCONTEXT, 0, &defcontext);
#endif
#if 0
      g_print("DEFCONTEXT:\n"); print_lc(&defcontext);
#endif
      WTInfo (WTI_INTERFACE, IFC_NDEVICES, &ndevices);
      WTInfo (WTI_INTERFACE, IFC_NCURSORS, &ncursors);

      /* Create a dummy window to receive wintab events */
      wa.wclass = GDK_INPUT_OUTPUT;
      wa.event_mask = GDK_ALL_EVENTS_MASK;
      wa.width = 2;
      wa.height = 2;
      wa.x = -100;
      wa.y = -100;
      wa.window_type = GDK_WINDOW_TOPLEVEL;
      if ((wintab_window = gdk_window_new (NULL, &wa, GDK_WA_X|GDK_WA_Y)) == NULL)
	{
	  g_warning ("gdk_input_init: gdk_window_new failed");
	  return;
	}
      gdk_window_ref (wintab_window);
      window_private = (GdkWindowPrivate *) wintab_window;
      
      for (i = 0; i < ndevices; i++)
	{
	  LOGCONTEXT lc;

	  WTInfo (WTI_DEVICES + i, DVC_NAME, devname);
      
	  WTInfo (WTI_DEVICES + i, DVC_NCSRTYPES, &ncsrtypes);
	  WTInfo (WTI_DEVICES + i, DVC_FIRSTCSR, &firstcsr);
	  WTInfo (WTI_DEVICES + i, DVC_HARDWARE, &hardware);
	  WTInfo (WTI_DEVICES + i, DVC_X, &axis_x);
	  WTInfo (WTI_DEVICES + i, DVC_Y, &axis_y);
	  WTInfo (WTI_DEVICES + i, DVC_NPRESSURE, &axis_npressure);
	  WTInfo (WTI_DEVICES + i, DVC_ORIENTATION, axis_orientation);

	  if (HIBYTE (specversion) > 1 || LOBYTE (specversion) >= 1)
	    {
	      WTInfo (WTI_DDCTXS + i, CTX_NAME, lc.lcName);
	      WTInfo (WTI_DDCTXS + i, CTX_OPTIONS, &lc.lcOptions);
	      lc.lcOptions |= CXO_MESSAGES;
	      lc.lcStatus = 0;
	      WTInfo (WTI_DDCTXS + i, CTX_LOCKS, &lc.lcLocks);
	      lc.lcMsgBase = WT_DEFBASE;
	      lc.lcDevice = i;
	      lc.lcPktRate = 20;
	      lc.lcPktData = PACKETDATA;
	      lc.lcPktMode = PK_BUTTONS; /* We want buttons in relative mode */
	      lc.lcMoveMask = PACKETDATA;
	      lc.lcBtnDnMask = lc.lcBtnUpMask = ~0;
	      WTInfo (WTI_DDCTXS + i, CTX_INORGX, &lc.lcInOrgX);
	      WTInfo (WTI_DDCTXS + i, CTX_INORGY, &lc.lcInOrgY);
	      WTInfo (WTI_DDCTXS + i, CTX_INORGZ, &lc.lcInOrgZ);
	      WTInfo (WTI_DDCTXS + i, CTX_INEXTX, &lc.lcInExtX);
	      WTInfo (WTI_DDCTXS + i, CTX_INEXTY, &lc.lcInExtY);
	      WTInfo (WTI_DDCTXS + i, CTX_INEXTZ, &lc.lcInExtZ);
	      lc.lcOutOrgX = axis_x.axMin;
	      lc.lcOutOrgY = axis_y.axMin;
	      lc.lcOutExtX = axis_x.axMax - axis_x.axMin;
	      lc.lcOutExtY = axis_y.axMax - axis_y.axMin;
	      lc.lcOutExtY = -lc.lcOutExtY; /* We want Y growing downward */
	      WTInfo (WTI_DDCTXS + i, CTX_SENSX, &lc.lcSensX);
	      WTInfo (WTI_DDCTXS + i, CTX_SENSY, &lc.lcSensY);
	      WTInfo (WTI_DDCTXS + i, CTX_SENSZ, &lc.lcSensZ);
	      WTInfo (WTI_DDCTXS + i, CTX_SYSMODE, &lc.lcSysMode);
	      lc.lcSysOrgX = lc.lcSysOrgY = 0;
	      WTInfo (WTI_DDCTXS + i, CTX_SYSEXTX, &lc.lcSysExtX);
	      WTInfo (WTI_DDCTXS + i, CTX_SYSEXTY, &lc.lcSysExtY);
	      WTInfo (WTI_DDCTXS + i, CTX_SYSSENSX, &lc.lcSysSensX);
	      WTInfo (WTI_DDCTXS + i, CTX_SYSSENSY, &lc.lcSysSensY);
	    }
	  else
	    {
	      lc = defcontext;
	      lc.lcOptions |= CXO_MESSAGES;
	      lc.lcMsgBase = WT_DEFBASE;
	      lc.lcPktRate = 20; /* Slow down the packets a bit */
	      lc.lcPktData = PACKETDATA;
	      lc.lcPktMode = PACKETMODE;
	      lc.lcMoveMask = PACKETDATA;
	      lc.lcBtnUpMask = lc.lcBtnDnMask = ~0;
#if 0
	      lc.lcOutExtY = -lc.lcOutExtY; /* Y grows downward */
#else
	      lc.lcOutOrgX = axis_x.axMin;
	      lc.lcOutOrgY = axis_y.axMin;
	      lc.lcOutExtX = axis_x.axMax - axis_x.axMin;
	      lc.lcOutExtY = axis_y.axMax - axis_y.axMin;
	      lc.lcOutExtY = -lc.lcOutExtY; /* We want Y growing downward */
#endif
	    }
#if 0
	  g_print("context for device %d:\n", i); print_lc(&lc);
#endif
	  hctx = g_new (HCTX, 1);
          if ((*hctx = WTOpen (window_private->xwindow, &lc, TRUE)) == NULL)
	    {
	      g_warning ("gdk_input_init: WTOpen failed");
	      return;
	    }
	  GDK_NOTE (MISC, g_print ("gdk_input_init: opened Wintab device %d %#x\n",
				   i, *hctx));

	  wintab_contexts = g_list_append (wintab_contexts, hctx);
#if 0
	  WTEnable (*hctx, TRUE);
#endif
	  WTOverlap (*hctx, TRUE);

#if 0
	  g_print("context for device %d after WTOpen:\n", i); print_lc(&lc);
#endif
	  for (j = firstcsr; j < firstcsr + ncsrtypes; j++)
	    {
	      gdkdev = g_new (GdkDevicePrivate, 1);
	      WTInfo (WTI_CURSORS + j, CSR_NAME, csrname);
	      gdkdev->info.name = g_strconcat (devname, " ", csrname, NULL);
	      gdkdev->info.deviceid = deviceid_counter++;
	      gdkdev->info.source = GDK_SOURCE_PEN;
	      gdkdev->info.mode = GDK_MODE_SCREEN;
#if USE_SYSCONTEXT
	      gdkdev->info.has_cursor = TRUE;
#else
	      gdkdev->info.has_cursor = FALSE;
#endif
	      gdkdev->hctx = *hctx;
	      gdkdev->cursor = j;
	      WTInfo (WTI_CURSORS + j, CSR_PKTDATA, &gdkdev->pktdata);
	      gdkdev->info.num_axes = 0;
	      if (gdkdev->pktdata & PK_X)
		gdkdev->info.num_axes++;
	      if (gdkdev->pktdata & PK_Y)
		gdkdev->info.num_axes++;
	      if (gdkdev->pktdata & PK_NORMAL_PRESSURE)
		gdkdev->info.num_axes++;
	      if (gdkdev->pktdata & PK_ORIENTATION)
		gdkdev->info.num_axes += 2; /* x and y tilt */
	      WTInfo (WTI_CURSORS + j, CSR_NPBTNMARKS, &gdkdev->npbtnmarks);
	      gdkdev->axes = g_new (GdkAxisInfo, gdkdev->info.num_axes);
	      gdkdev->info.axes = g_new (GdkAxisUse, gdkdev->info.num_axes);
	      gdkdev->last_axis_data = g_new (gint, gdkdev->info.num_axes);
	      
	      for (k = 0; k < GDK_AXIS_LAST; k++)
		gdkdev->axis_for_use[k] = -1;

	      k = 0;
	      if (gdkdev->pktdata & PK_X)
		{
		  gdkdev->axes[k].xresolution =
		    gdkdev->axes[k].resolution = axis_x.axResolution / 65535.;
		  gdkdev->axes[k].xmin_value =
		    gdkdev->axes[k].min_value = axis_x.axMin;
		  gdkdev->axes[k].xmax_value =
		    gdkdev->axes[k].max_value = axis_x.axMax;
		  gdkdev->info.axes[k] = GDK_AXIS_X;
		  gdkdev->axis_for_use[GDK_AXIS_X] = k;
		  k++;
		}
	      if (gdkdev->pktdata & PK_Y)
		{
		  gdkdev->axes[k].xresolution =
		    gdkdev->axes[k].resolution = axis_y.axResolution / 65535.;
		  gdkdev->axes[k].xmin_value =
		    gdkdev->axes[k].min_value = axis_y.axMin;
		  gdkdev->axes[k].xmax_value =
		    gdkdev->axes[k].max_value = axis_y.axMax;
		  gdkdev->info.axes[k] = GDK_AXIS_Y;
		  gdkdev->axis_for_use[GDK_AXIS_Y] = k;
		  k++;
		}
	      if (gdkdev->pktdata & PK_NORMAL_PRESSURE)
		{
		  gdkdev->axes[k].xresolution =
		    gdkdev->axes[k].resolution = axis_npressure.axResolution / 65535.;
		  gdkdev->axes[k].xmin_value =
		    gdkdev->axes[k].min_value = axis_npressure.axMin;
		  gdkdev->axes[k].xmax_value =
		    gdkdev->axes[k].max_value = axis_npressure.axMax;
		  gdkdev->info.axes[k] = GDK_AXIS_PRESSURE;
		  gdkdev->axis_for_use[GDK_AXIS_PRESSURE] = k;
		  k++;
		}
	      if (gdkdev->pktdata & PK_ORIENTATION)
		{
		  GdkAxisUse axis;

		  gdkdev->orientation_axes[0] = axis_orientation[0];
		  gdkdev->orientation_axes[1] = axis_orientation[1];
		  for (axis = GDK_AXIS_XTILT; axis <= GDK_AXIS_YTILT; axis++)
		    {
		      gdkdev->axes[k].xresolution =
			gdkdev->axes[k].resolution = 1000;
		      gdkdev->axes[k].xmin_value =
			gdkdev->axes[k].min_value = -1000;
		      gdkdev->axes[k].xmax_value =
			gdkdev->axes[k].max_value = 1000;
		      gdkdev->info.axes[k] = axis;
		      gdkdev->axis_for_use[axis] = k;
		      k++;
		    }
		}
	      gdkdev->info.num_keys = 0;
	      gdkdev->info.keys = NULL;
	      GDK_NOTE (EVENTS,
			g_print ("gdk_input_init: device: %d axes: %d\n",
				 gdkdev->info.deviceid,
				 gdkdev->info.num_axes));
	      gdk_input_devices = g_list_append (gdk_input_devices,
						 gdkdev);
	    }
	}
      gdk_input_vtable.set_mode           = gdk_input_win32_set_mode;
      gdk_input_vtable.set_axes           = NULL;
      gdk_input_vtable.set_key            = NULL;
      gdk_input_vtable.motion_events      = NULL;
      gdk_input_vtable.get_pointer	  = gdk_input_win32_get_pointer;
      gdk_input_vtable.grab_pointer	  = gdk_input_win32_grab_pointer;
      gdk_input_vtable.ungrab_pointer     = gdk_input_win32_ungrab_pointer;
      gdk_input_vtable.configure_event    = gdk_input_win32_configure_event;
      gdk_input_vtable.enter_event        = gdk_input_win32_enter_event;
      gdk_input_vtable.other_event        = gdk_input_win32_other_event;
      gdk_input_vtable.enable_window      = gdk_input_win32_enable_window;
      gdk_input_vtable.disable_window     = gdk_input_win32_disable_window;

      gdk_input_root_width = gdk_screen_width ();
      gdk_input_root_height = gdk_screen_height ();
      gdk_input_ignore_core = FALSE;
    }
  else
    {
      gdk_input_vtable.set_mode           = NULL;
      gdk_input_vtable.set_axes           = NULL;
      gdk_input_vtable.set_key            = NULL;
      gdk_input_vtable.motion_events      = NULL;
      gdk_input_vtable.get_pointer	  = gdk_input_none_get_pointer;
      gdk_input_vtable.grab_pointer	  = NULL;
      gdk_input_vtable.ungrab_pointer     = NULL;
      gdk_input_vtable.configure_event    = NULL;
      gdk_input_vtable.enter_event        = NULL;
      gdk_input_vtable.other_event        = NULL;
      gdk_input_vtable.enable_window      = NULL;
      gdk_input_vtable.disable_window     = NULL;
      gdk_input_ignore_core = FALSE;
    }
  
  gdk_input_devices = g_list_append (gdk_input_devices, &gdk_input_core_info);
}

static void
gdk_input_get_root_relative_geometry (HWND w,
				      int  *x_ret,
				      int  *y_ret)
{
  RECT rect;

  GetWindowRect (w, &rect);

  if (x_ret)
    *x_ret = rect.left;
  if (y_ret)
    *y_ret = rect.top;
}

static void
gdk_input_translate_coordinates (GdkDevicePrivate *gdkdev,
				 GdkInputWindow   *input_window,
				 gint             *axis_data,
				 gdouble          *x,
				 gdouble          *y,
				 gdouble          *pressure,
				 gdouble          *xtilt,
				 gdouble          *ytilt)
{
  GdkWindowPrivate *window_private;
  gint x_axis, y_axis, pressure_axis, xtilt_axis, ytilt_axis;
  gdouble device_width, device_height;
  gdouble x_offset, y_offset, x_scale, y_scale;

  window_private = (GdkWindowPrivate *) input_window->window;

  x_axis = gdkdev->axis_for_use[GDK_AXIS_X];
  y_axis = gdkdev->axis_for_use[GDK_AXIS_Y];
  pressure_axis = gdkdev->axis_for_use[GDK_AXIS_PRESSURE];
  xtilt_axis = gdkdev->axis_for_use[GDK_AXIS_XTILT];
  ytilt_axis = gdkdev->axis_for_use[GDK_AXIS_YTILT];

  device_width = gdkdev->axes[x_axis].max_value - 
		   gdkdev->axes[x_axis].min_value;
  device_height = gdkdev->axes[y_axis].max_value - 
                    gdkdev->axes[y_axis].min_value;

  if (gdkdev->info.mode == GDK_MODE_SCREEN) 
    {
      x_scale = gdk_input_root_width / device_width;
      y_scale = gdk_input_root_height / device_height;

      x_offset = -input_window->root_x;
      y_offset = -input_window->root_y;
    }
  else				/* GDK_MODE_WINDOW */
    {
      double device_aspect = (device_height*gdkdev->axes[y_axis].resolution) /
	(device_width*gdkdev->axes[x_axis].resolution);

      if (device_aspect * window_private->width >= window_private->height)
	{
	  /* device taller than window */
	  x_scale = window_private->width / device_width;
	  y_scale = (x_scale * gdkdev->axes[x_axis].resolution)
	    / gdkdev->axes[y_axis].resolution;

	  x_offset = 0;
	  y_offset = -(device_height * y_scale - 
			       window_private->height)/2;
	}
      else
	{
	  /* window taller than device */
	  y_scale = window_private->height / device_height;
	  x_scale = (y_scale * gdkdev->axes[y_axis].resolution)
	    / gdkdev->axes[x_axis].resolution;

	  y_offset = 0;
	  x_offset = -(device_width * x_scale - window_private->width)/2;
	}
    }
  
  if (x)
    *x = x_offset + x_scale*axis_data[x_axis];
  if (y)
    *y = y_offset + y_scale*axis_data[y_axis];

  if (pressure)
    {
      if (pressure_axis != -1)
	*pressure = ((double)axis_data[pressure_axis] 
		     - gdkdev->axes[pressure_axis].min_value) 
	  / (gdkdev->axes[pressure_axis].max_value 
	     - gdkdev->axes[pressure_axis].min_value);
      else
	*pressure = 0.5;
    }

  if (xtilt)
    {
      if (xtilt_axis != -1)
	{
	  *xtilt = 2. * (double)(axis_data[xtilt_axis] - 
				 (gdkdev->axes[xtilt_axis].min_value +
				  gdkdev->axes[xtilt_axis].max_value)/2) /
	    (gdkdev->axes[xtilt_axis].max_value -
	     gdkdev->axes[xtilt_axis].min_value);
	}
      else
	*xtilt = 0;
    }
  
  if (ytilt)
    {
      if (ytilt_axis != -1)
	{
	  *ytilt = 2. * (double)(axis_data[ytilt_axis] - 
				 (gdkdev->axes[ytilt_axis].min_value +
				  gdkdev->axes[ytilt_axis].max_value)/2) /
	    (gdkdev->axes[ytilt_axis].max_value -
	     gdkdev->axes[ytilt_axis].min_value);
	}
      else
	*ytilt = 0;
    }
}

gint
gdk_input_set_mode (guint32      deviceid,
		    GdkInputMode mode)
{
  if (deviceid == GDK_CORE_POINTER)
    return FALSE;

  if (gdk_input_vtable.set_mode)
    return gdk_input_vtable.set_mode (deviceid, mode);
  else
    return FALSE;
}

void
gdk_input_set_axes (guint32     deviceid,
		    GdkAxisUse *axes)
{
  int i;
  GdkDevicePrivate *gdkdev = gdk_input_find_device (deviceid);
  g_return_if_fail (gdkdev != NULL);

  if (deviceid == GDK_CORE_POINTER)
    return;

  for (i = GDK_AXIS_IGNORE; i < GDK_AXIS_LAST; i++)
    {
      gdkdev->axis_for_use[i] = -1;
    }

  for (i = 0; i < gdkdev->info.num_axes; i++)
    {
      gdkdev->info.axes[i] = axes[i];
      gdkdev->axis_for_use[axes[i]] = i;
    }
}

static void 
gdk_input_win32_get_pointer (GdkWindow       *window,
			     guint32	      deviceid,
			     gdouble         *x,
			     gdouble         *y,
			     gdouble         *pressure,
			     gdouble         *xtilt,
			     gdouble         *ytilt,
			     GdkModifierType *mask)
{
  GdkDevicePrivate *gdkdev;
  GdkInputWindow *input_window;
  gint x_int, y_int;
  gint i;

  if (deviceid == GDK_CORE_POINTER)
    {
      gdk_window_get_pointer (window, &x_int, &y_int, mask);
      if (x)
	*x = x_int;
      if (y)
	*y = y_int;
      if (pressure)
	*pressure = 0.5;
      if (xtilt)
	*xtilt = 0;
      if (ytilt)
	*ytilt = 0;
    }
  else
    {
      if (mask)
	gdk_window_get_pointer (window, NULL, NULL, mask);
      
      gdkdev = gdk_input_find_device (deviceid);
      g_return_if_fail (gdkdev != NULL);

      input_window = gdk_input_window_find (window);
      g_return_if_fail (input_window != NULL);

      gdk_input_translate_coordinates (gdkdev, input_window,
				       gdkdev->last_axis_data,
				       x, y, pressure,
				       xtilt, ytilt);
      if (mask)
	{
	  *mask &= 0xFF;
	  *mask |= ((gdkdev->last_buttons & 0x1F) << 8);
	}
    }
}

static void
gdk_input_none_get_pointer (GdkWindow       *window,
			    guint32          deviceid,
			    gdouble         *x,
			    gdouble         *y,
			    gdouble         *pressure,
			    gdouble         *xtilt,
			    gdouble         *ytilt,
			    GdkModifierType *mask)
{
  gint x_int, y_int;

  gdk_window_get_pointer (window, &x_int, &y_int, mask);

  if (x)
    *x = x_int;
  if (y)
    *y = y_int;
  if (pressure)
    *pressure = 0.5;
  if (xtilt)
    *xtilt = 0;
  if (ytilt)
    *ytilt = 0;
}

static gint
gdk_input_win32_set_mode (guint32      deviceid,
			  GdkInputMode mode)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;
  GdkInputMode old_mode;
  GdkInputWindow *input_window;

  if (deviceid == GDK_CORE_POINTER)
    return FALSE;

  gdkdev = gdk_input_find_device (deviceid);
  g_return_val_if_fail (gdkdev != NULL, FALSE);
  old_mode = gdkdev->info.mode;

  if (old_mode == mode)
    return TRUE;

  gdkdev->info.mode = mode;

  if (mode == GDK_MODE_WINDOW)
    {
      gdkdev->info.has_cursor = FALSE;
      for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    gdk_input_win32_enable_window (input_window->window, gdkdev);
	  else
	    if (old_mode != GDK_MODE_DISABLED)
	      gdk_input_win32_disable_window (input_window->window, gdkdev);
	}
    }
  else if (mode == GDK_MODE_SCREEN)
    {
      gdkdev->info.has_cursor = TRUE;
      for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	gdk_input_win32_enable_window (((GdkInputWindow *)tmp_list->data)->window,
				       gdkdev);
    }
  else  /* mode == GDK_MODE_DISABLED */
    {
      for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (old_mode != GDK_MODE_WINDOW ||
	      input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    gdk_input_win32_disable_window (input_window->window, gdkdev);
	}
    }

  return TRUE;
}

static void
gdk_input_win32_configure_event (GdkEventConfigure *event,
				 GdkWindow         *window)
{
  GdkInputWindow *input_window;
  gint root_x, root_y;

  input_window = gdk_input_window_find (window);
  g_return_if_fail (window != NULL);

  gdk_input_get_root_relative_geometry (GDK_WINDOW_XWINDOW (window),
					&root_x, &root_y);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

static void 
gdk_input_win32_enter_event (GdkEventCrossing *event, 
			     GdkWindow        *window)
{
  GdkInputWindow *input_window;
  gint root_x, root_y;

  input_window = gdk_input_window_find (window);
  g_return_if_fail (window != NULL);

  gdk_input_get_root_relative_geometry (GDK_WINDOW_XWINDOW (window),
					&root_x, &root_y);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

static void
decode_tilt (gint   *axis_data,
	     AXIS   *axes,
	     PACKET *packet)
{
  /* As I don't have a tilt-sensing tablet,
   * I cannot test this code.
   */
  
  double az, el;

  az = TWOPI * packet->pkOrientation.orAzimuth /
    (axes[0].axResolution / 65536.);
  el = TWOPI * packet->pkOrientation.orAltitude /
    (axes[1].axResolution / 65536.);
  
  /* X tilt */
  axis_data[0] = cos (az) * cos (el) * 1000;
  /* Y tilt */
  axis_data[1] = sin (az) * cos (el) * 1000;
}

static gint 
gdk_input_win32_other_event (GdkEvent  *event,
			     MSG       *xevent)
{
  GdkWindow *current_window;
  GdkInputWindow *input_window;
  GdkWindow *window;
  GdkWindowPrivate *window_private;
  GdkDevicePrivate *gdkdev;
  GdkEventMask masktest;
  POINT pt;
  PACKET packet;
  gint return_val;
  gint k;
  gint x, y;

  if (event->any.window != wintab_window)
    g_warning ("gdk_input_win32_other_event: not wintab_window?");

#if USE_SYSCONTEXT
  window = gdk_window_at_pointer (&x, &y);
  if (window == NULL)
    window = (GdkWindow *) &gdk_root_parent;

  gdk_window_ref (window);

  window_private = (GdkWindowPrivate *) window;

  GDK_NOTE (EVENTS, g_print ("gdk_input_win32_other_event: window=%#x (%d,%d)\n", window_private->xwindow, x, y));
  
#else
  /* ??? This code is pretty bogus */
  current_window = gdk_window_lookup (GetActiveWindow ());
  if (current_window == NULL)
    return FALSE;
  
  input_window = gdk_input_window_find_within (current_window);
  if (input_window == NULL)
    return FALSE;
#endif

  if (xevent->message == WT_PACKET)
    {
      if (!WTPacket ((HCTX) xevent->lParam, xevent->wParam, &packet))
	return FALSE;
    }

  switch (xevent->message)
    {
    case WT_PACKET:
      if (window_private == &gdk_root_parent)
	{
	  GDK_NOTE (EVENTS, g_print ("...is root\n"));
	  return FALSE;
	}

      if ((gdkdev = gdk_input_find_dev_from_ctx ((HCTX) xevent->lParam,
						 packet.pkCursor)) == NULL)
	return FALSE;

      if (gdkdev->info.mode == GDK_MODE_DISABLED)
	return FALSE;
      
      k = 0;
      if (gdkdev->pktdata & PK_X)
	gdkdev->last_axis_data[k++] = packet.pkX;
      if (gdkdev->pktdata & PK_Y)
	gdkdev->last_axis_data[k++] = packet.pkY;
      if (gdkdev->pktdata & PK_NORMAL_PRESSURE)
	gdkdev->last_axis_data[k++] = packet.pkNormalPressure;
      if (gdkdev->pktdata & PK_ORIENTATION)
	{
	  decode_tilt (gdkdev->last_axis_data + k,
		       gdkdev->orientation_axes, &packet);
	  k += 2;
	}

      g_assert (k == gdkdev->info.num_axes);

      if (HIWORD (packet.pkButtons) != TBN_NONE)
	{
	  /* Gdk buttons are numbered 1.. */
	  event->button.button = 1 + LOWORD (packet.pkButtons);

	  if (HIWORD (packet.pkButtons) == TBN_UP)
	    {
	      event->any.type = GDK_BUTTON_RELEASE;
	      masktest = GDK_BUTTON_RELEASE_MASK;
	      gdkdev->button_state &= ~(1 << LOWORD (packet.pkButtons));
	    }
	  else
	    {
	      event->any.type = GDK_BUTTON_PRESS;
	      masktest = GDK_BUTTON_PRESS_MASK;
	      gdkdev->button_state |= 1 << LOWORD (packet.pkButtons);
	    }
	}
      else
	{
	  event->any.type = GDK_MOTION_NOTIFY;
	  masktest = GDK_POINTER_MOTION_MASK;
	  if (gdkdev->button_state & (1 << 0))
	    masktest |= GDK_BUTTON_MOTION_MASK | GDK_BUTTON1_MOTION_MASK;
	  if (gdkdev->button_state & (1 << 1))
	    masktest |= GDK_BUTTON_MOTION_MASK | GDK_BUTTON2_MOTION_MASK;
	  if (gdkdev->button_state & (1 << 2))
	    masktest |= GDK_BUTTON_MOTION_MASK | GDK_BUTTON3_MOTION_MASK;
	}

      /* Now we can check if the window wants the event, and
       * propagate if necessary.
       */
    dijkstra:
      if (!window_private->extension_events_selected
	  || !(window_private->extension_events & masktest))
	{
	  GDK_NOTE (EVENTS, g_print ("...not selected\n"));

	  if (window_private->parent == (GdkWindow *) &gdk_root_parent)
	    return FALSE;
	  
	  pt.x = x;
	  pt.y = y;
	  ClientToScreen (window_private->xwindow, &pt);
	  gdk_window_unref (window);
	  window = window_private->parent;
	  gdk_window_ref (window);
	  window_private = (GdkWindowPrivate *) window;
	  ScreenToClient (window_private->xwindow, &pt);
	  x = pt.x;
	  y = pt.y;
	  GDK_NOTE (EVENTS, g_print ("...propagating to %#x, (%d,%d)\n", window_private->xwindow, x, y));
	  goto dijkstra;
	}

      input_window = gdk_input_window_find (window);

      g_assert (input_window != NULL);

      if (gdkdev->info.mode == GDK_MODE_WINDOW
	  && input_window->mode == GDK_EXTENSION_EVENTS_CURSOR)
	return FALSE;

      event->any.window = window;

      if (event->any.type == GDK_BUTTON_PRESS
	  || event->any.type == GDK_BUTTON_RELEASE)
	{
	  event->button.time = xevent->time;
	  event->button.source = gdkdev->info.source;
	  last_moved_cursor_id = 
	    event->button.deviceid = gdkdev->info.deviceid;
	  
#if 0
#if USE_SYSCONTEXT
	  /* Buttons 1 to 3 will come in as WM_[LMR]BUTTON{DOWN,UP} */
	  if (event->button.button <= 3)
	    return FALSE;
#endif
#endif
	  gdk_input_translate_coordinates (gdkdev, input_window,
					   gdkdev->last_axis_data,
					   &event->button.x, &event->button.y,
					   &event->button.pressure,
					   &event->button.xtilt, 
					   &event->button.ytilt);

	  event->button.state = ((gdkdev->button_state << 8)
				 & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
				    | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
				    | GDK_BUTTON5_MASK));
	  GDK_NOTE (EVENTS, g_print ("WINTAB button %s: %d %d %g,%g %g\n",
				     (event->button.type == GDK_BUTTON_PRESS ?
				      "press" : "release"),
				     event->button.deviceid,
				     event->button.button,
				     event->button.x, event->button.y,
				     event->button.pressure));
	}
      else
	{
	  event->motion.time = xevent->time;
	  last_moved_cursor_id =
	    event->motion.deviceid = gdkdev->info.deviceid;
	  event->motion.is_hint = FALSE;
	  event->motion.source = gdkdev->info.source;

	  gdk_input_translate_coordinates (gdkdev, input_window,
					   gdkdev->last_axis_data,
					   &event->motion.x, &event->motion.y,
					   &event->motion.pressure,
					   &event->motion.xtilt, 
					   &event->motion.ytilt);

	  event->motion.state = ((gdkdev->button_state << 8)
				 & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
				    | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
				    | GDK_BUTTON5_MASK));

	  GDK_NOTE (EVENTS, g_print ("WINTAB motion: %d %g,%g %g\n",
				     event->motion.deviceid,
				     event->motion.x, event->motion.y,
				     event->motion.pressure));

	  /* Check for missing release or press events for the normal
	   * pressure button. At least on my ArtPadII I sometimes miss a
	   * release event?
	   */
	  if ((gdkdev->pktdata & PK_NORMAL_PRESSURE
	       && (event->motion.state & GDK_BUTTON1_MASK)
	       && packet.pkNormalPressure <= MAX (0, gdkdev->npbtnmarks[0] - 2))
	      || (gdkdev->pktdata & PK_NORMAL_PRESSURE
		  && !(event->motion.state & GDK_BUTTON1_MASK)
		  && packet.pkNormalPressure > gdkdev->npbtnmarks[1] + 2))
	    {
	      GdkEvent *event2 = gdk_event_copy (event);
	      if (event->motion.state & GDK_BUTTON1_MASK)
		{
		  event2->button.type = GDK_BUTTON_RELEASE;
		  gdkdev->button_state &= ~1;
		}
	      else
		{
		  event2->button.type = GDK_BUTTON_PRESS;
		  gdkdev->button_state |= 1;
		}
	      event2->button.state = ((gdkdev->button_state << 8)
				      & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK
					 | GDK_BUTTON3_MASK | GDK_BUTTON4_MASK
					 | GDK_BUTTON5_MASK));
	      event2->button.button = 1;
	      GDK_NOTE (EVENTS, g_print ("WINTAB synthesized button %s: %d %d %g,%g %g\n",
					 (event2->button.type == GDK_BUTTON_PRESS ?
					  "press" : "release"),
					 event2->button.deviceid,
					 event2->button.button,
					 event2->button.x, event2->button.y,
					 event2->button.pressure));
	      gdk_event_queue_append (event2);
	    }
	}
      return TRUE;

    case WT_PROXIMITY:
      if (LOWORD (xevent->lParam) == 0)
	{
	  event->proximity.type = GDK_PROXIMITY_OUT;
	  gdk_input_ignore_core = FALSE;
	}
      else
	{
	  event->proximity.type = GDK_PROXIMITY_IN;
	  gdk_input_ignore_core = TRUE;
	}
      event->proximity.time = xevent->time;
      event->proximity.source = GDK_SOURCE_PEN;
      event->proximity.deviceid = last_moved_cursor_id;

      GDK_NOTE (EVENTS, g_print ("WINTAB proximity %s: %d\n",
				 (event->proximity.type == GDK_PROXIMITY_IN ?
				  "in" : "out"),
				 event->proximity.deviceid));
      return TRUE;
    }

  return FALSE;
}

static gint
gdk_input_win32_enable_window (GdkWindow        *window,
			       GdkDevicePrivate *gdkdev)
{
  GdkWindowPrivate *window_private = (GdkWindowPrivate *) window;

  window_private->extension_events_selected = TRUE;
  return TRUE;
}

static gint
gdk_input_win32_disable_window (GdkWindow        *window,
			        GdkDevicePrivate *gdkdev)
{
  GdkWindowPrivate *window_private = (GdkWindowPrivate *) window;

  window_private->extension_events_selected = FALSE;
  return TRUE;
}

static gint
gdk_input_win32_grab_pointer (GdkWindow    *window,
			      gint          owner_events,
			      GdkEventMask  event_mask,
			      GdkWindow    *confine_to,
			      guint32       time)
{
  GdkInputWindow *input_window, *new_window;
  gboolean need_ungrab;
  GdkDevicePrivate *gdkdev;
  GList *tmp_list;
  gint result;

  tmp_list = gdk_input_windows;
  new_window = NULL;
  need_ungrab = FALSE;

  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;

      if (input_window->window == window)
	new_window = input_window;
      else if (input_window->grabbed)
	{
	  input_window->grabbed = FALSE;
	  need_ungrab = TRUE;
	}

      tmp_list = tmp_list->next;
    }

  if (new_window)
    {
      new_window->grabbed = TRUE;
      
      tmp_list = gdk_input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (gdkdev->info.deviceid != GDK_CORE_POINTER)
	    {
#if 0	      
	      gdk_input_find_events (window, gdkdev,
				     event_mask,
				     event_classes, &num_classes);
	      result = XGrabDevice (GDK_DISPLAY(), gdkdev->xdevice,
				    GDK_WINDOW_XWINDOW (window),
				    owner_events, num_classes, event_classes,
				    GrabModeAsync, GrabModeAsync, time);
	      
	      /* FIXME: if failure occurs on something other than the first
		 device, things will be badly inconsistent */
	      if (result != Success)
		return result;
#endif
	    }
	  tmp_list = tmp_list->next;
	}
    }
  else
    { 
      tmp_list = gdk_input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (gdkdev->info.deviceid != GDK_CORE_POINTER && 
	      ((gdkdev->button_state != 0) || need_ungrab))
	    {
#if 0
	      XUngrabDevice (gdk_display, gdkdev->xdevice, time);
#endif
	      gdkdev->button_state = 0;
	    }
	  
	  tmp_list = tmp_list->next;
	}
    }

  return Success;
      
}

static void 
gdk_input_win32_ungrab_pointer (guint32 time)
{
  GdkInputWindow *input_window;
  GdkDevicePrivate *gdkdev;
  GList *tmp_list;

  tmp_list = gdk_input_windows;
  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;
      if (input_window->grabbed)
	break;
      tmp_list = tmp_list->next;
    }

  if (tmp_list)			/* we found a grabbed window */
    {
      input_window->grabbed = FALSE;

      tmp_list = gdk_input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
#if 0
	  if (gdkdev->info.deviceid != GDK_CORE_POINTER && gdkdev->xdevice)
	    XUngrabDevice (gdk_display, gdkdev->xdevice, time);
#endif
	  tmp_list = tmp_list->next;
	}
    }
}

GList *
gdk_input_list_devices (void)
{
  return gdk_input_devices;
}

void
gdk_input_set_source (guint32        deviceid,
		      GdkInputSource source)
{
  GdkDevicePrivate *gdkdev = gdk_input_find_device (deviceid);
  g_return_if_fail (gdkdev != NULL);

  gdkdev->info.source = source;
}

void gdk_input_set_key (guint32 deviceid,
			guint   index,
			guint   keyval,
			GdkModifierType modifiers)
{
  if (deviceid != GDK_CORE_POINTER && gdk_input_vtable.set_key)
    gdk_input_vtable.set_key (deviceid, index, keyval, modifiers);
}

GdkTimeCoord *
gdk_input_motion_events (GdkWindow *window,
			 guint32    deviceid,
			 guint32    start,
			 guint32    stop,
			 gint      *nevents_return)
{
  GdkWindowPrivate *window_private;
  GdkTimeCoord *coords;
  int i;

  g_return_val_if_fail (window != NULL, NULL);
  window_private = (GdkWindowPrivate *) window;
  if (window_private->destroyed)
    return NULL;

  *nevents_return = 0;
  return NULL;		/* ??? */
}

static GdkInputWindow *
gdk_input_window_find (GdkWindow *window)
{
  GList *tmp_list;

  for (tmp_list=gdk_input_windows; tmp_list; tmp_list=tmp_list->next)
    if (((GdkInputWindow *)(tmp_list->data))->window == window)
      return (GdkInputWindow *)(tmp_list->data);

  return NULL;      /* Not found */
}

static GdkInputWindow *
gdk_input_window_find_within (GdkWindow *window)
{
  GList *tmp_list;
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *tmp_private;
  GdkInputWindow *candidate = NULL;

  window_private = (GdkWindowPrivate *) window;

  for (tmp_list=gdk_input_windows; tmp_list; tmp_list=tmp_list->next)
    {
      (GdkWindowPrivate *) tmp_private =
	(GdkWindowPrivate *) (((GdkInputWindow *)(tmp_list->data))->window);
      if (tmp_private == window_private
	  || IsChild (window_private->xwindow, tmp_private->xwindow))
	{
	  if (candidate)
	    return NULL;		/* Multiple hits */
	  candidate = (GdkInputWindow *)(tmp_list->data);
	}
    }

  return candidate;
}

/* FIXME: this routine currently needs to be called between creation
   and the corresponding configure event (because it doesn't get the
   root_relative_geometry).  This should work with
   gtk_window_set_extension_events, but will likely fail in other
   cases */

void
gdk_input_set_extension_events (GdkWindow       *window,
				gint             mask,
				GdkExtensionMode mode)
{
  GdkWindowPrivate *window_private;
  GList *tmp_list;
  GdkInputWindow *iw;

  g_return_if_fail (window != NULL);
  window_private = (GdkWindowPrivate *) window;
  if (window_private->destroyed)
    return;

  if (mode == GDK_EXTENSION_EVENTS_NONE)
    mask = 0;

  if (mask != 0)
    {
      iw = g_new (GdkInputWindow,1);

      iw->window = window;
      iw->mode = mode;

      iw->grabbed = FALSE;

      gdk_input_windows = g_list_append (gdk_input_windows, iw);
      window_private->extension_events = mask;

      /* Add enter window events to the event mask */
      gdk_window_set_events (window,
			     gdk_window_get_events (window) | 
			     GDK_ENTER_NOTIFY_MASK);
    }
  else
    {
      iw = gdk_input_window_find (window);
      if (iw)
	{
	  gdk_input_windows = g_list_remove (gdk_input_windows, iw);
	  g_free (iw);
	}

      window_private->extension_events = 0;
    }

  for (tmp_list = gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDevicePrivate *gdkdev = (GdkDevicePrivate *)(tmp_list->data);

      if (gdkdev->info.deviceid != GDK_CORE_POINTER)
	{
	  if (mask != 0 && gdkdev->info.mode != GDK_MODE_DISABLED
	      && (gdkdev->info.has_cursor || mode == GDK_EXTENSION_EVENTS_ALL))
	    gdk_input_win32_enable_window (window, gdkdev);
	  else
	    gdk_input_win32_disable_window (window, gdkdev);
	}
    }
}

void
gdk_input_window_destroy (GdkWindow *window)
{
  GdkInputWindow *input_window;

  input_window = gdk_input_window_find (window);
  g_return_if_fail (input_window != NULL);

  gdk_input_windows = g_list_remove (gdk_input_windows,input_window);
  g_free (input_window);
}

void
gdk_input_exit (void)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;

  for (tmp_list = gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev->info.deviceid != GDK_CORE_POINTER)
	{
	  gdk_input_win32_set_mode (gdkdev->info.deviceid, GDK_MODE_DISABLED);
	  g_free (gdkdev->info.name);
	  g_free (gdkdev->last_axis_data);
	  g_free (gdkdev->info.axes);
	  g_free (gdkdev->info.keys);
	  g_free (gdkdev->axes);
	  g_free (gdkdev);
	}
    }

  g_list_free (gdk_input_devices);

  for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      g_free (tmp_list->data);
    }
  g_list_free (gdk_input_windows);

#if 1
  for (tmp_list = wintab_contexts; tmp_list; tmp_list = tmp_list->next)
    {
      HCTX *hctx = (HCTX *) tmp_list->data;
      BOOL result;

      /* For some reason WTEnable and/or WTClose tend to crash here.
       * Protect with __try/__except to avoid a message box.
       */
      __try {
#if 0
        WTEnable (*hctx, FALSE);
#endif
	result = WTClose (*hctx);
      }
      __except (/* GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? */
                EXCEPTION_EXECUTE_HANDLER /*: 
                EXCEPTION_CONTINUE_SEARCH */) {
	result = FALSE;
      }
      if (!result)
	g_warning ("gdk_input_exit: Closing Wintab context %#x failed", *hctx);
      g_free (hctx);
    }
#endif
  g_list_free (wintab_contexts);
}

static GdkDevicePrivate *
gdk_input_find_device (guint32 id)
{
  GList *tmp_list = gdk_input_devices;
  GdkDevicePrivate *gdkdev;

  while (tmp_list)
    {
      gdkdev = (GdkDevicePrivate *) (tmp_list->data);
      if (gdkdev->info.deviceid == id)
	return gdkdev;
      tmp_list = tmp_list->next;
    }
  return NULL;
}

static GdkDevicePrivate *
gdk_input_find_dev_from_ctx (HCTX hctx,
			     UINT cursor)
{
  GList *tmp_list = gdk_input_devices;
  GdkDevicePrivate *gdkdev;

  while (tmp_list)
    {
      gdkdev = (GdkDevicePrivate *) (tmp_list->data);
      if (gdkdev->hctx == hctx && gdkdev->cursor == cursor)
	return gdkdev;
      tmp_list = tmp_list->next;
    }
  return NULL;
}

void
gdk_input_window_get_pointer (GdkWindow       *window,
			      guint32	       deviceid,
			      gdouble         *x,
			      gdouble         *y,
			      gdouble         *pressure,
			      gdouble         *xtilt,
			      gdouble         *ytilt,
			      GdkModifierType *mask)
{
  if (gdk_input_vtable.get_pointer)
    gdk_input_vtable.get_pointer (window, deviceid, x, y, pressure,
				  xtilt, ytilt, mask);
}
