/* GdkQuartzView.m
 *
 * Copyright (C) 2005-2007 Imendio AB
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


#import "GdkQuartzView.h"
#include "gdkwindow-quartz.h"
#include "gdkprivate-quartz.h"

@implementation GdkQuartzView

-(void)setGdkWindow:(GdkWindow *)window
{
  gdk_window = window;
}

-(GdkWindow *)gdkWindow
{
  return gdk_window;
}

-(BOOL)isFlipped
{
  return YES;
}

-(BOOL)isOpaque
{
  if (GDK_WINDOW_DESTROYED (gdk_window))
    return YES;

  /* A view is opaque if its GdkWindow doesn't have the RGBA colormap */
  return gdk_drawable_get_colormap (gdk_window) != gdk_screen_get_rgba_colormap (_gdk_screen);
}

-(void)drawRect:(NSRect)rect 
{
  GdkRectangle gdk_rect;
  GdkWindowObject *private = GDK_WINDOW_OBJECT (gdk_window);
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  const NSRect *drawn_rects;
  int count, i;
  GdkRegion *region;

  if (GDK_WINDOW_DESTROYED (gdk_window))
    return;

  if (!(private->event_mask & GDK_EXPOSURE_MASK))
    return;

  if (NSEqualRects (rect, NSZeroRect))
    return;

  GDK_QUARTZ_ALLOC_POOL;

  [self getRectsBeingDrawn:&drawn_rects count:&count];

  region = gdk_region_new ();

  for (i = 0; i < count; i++)
    {
      gdk_rect.x = drawn_rects[i].origin.x;
      gdk_rect.y = drawn_rects[i].origin.y;
      gdk_rect.width = drawn_rects[i].size.width;
      gdk_rect.height = drawn_rects[i].size.height;

      gdk_region_union_with_rect (region, &gdk_rect);
    }

  if (!gdk_region_empty (region))
    {
      GdkEvent event;
      
      gdk_rect.x = rect.origin.x;
      gdk_rect.y = rect.origin.y;
      gdk_rect.width = rect.size.width;
      gdk_rect.height = rect.size.height;
      
      event.expose.type = GDK_EXPOSE;
      event.expose.window = g_object_ref (gdk_window);
      event.expose.send_event = FALSE;
      event.expose.count = 0;
      event.expose.region = region;
      event.expose.area = gdk_rect;
      
      impl->in_paint_rect_count++;

      (*_gdk_event_func) (&event, _gdk_event_data);

      impl->in_paint_rect_count--;

      g_object_unref (gdk_window);
    }

  gdk_region_destroy (region);

  if (needsInvalidateShadow)
    {
      [[self window] invalidateShadow];
      needsInvalidateShadow = NO;
    }

  GDK_QUARTZ_RELEASE_POOL;
}

-(void)setNeedsInvalidateShadow:(BOOL)invalidate
{
  needsInvalidateShadow = invalidate;
}

/* For information on setting up tracking rects properly, see here:
 * http://developer.apple.com/documentation/Cocoa/Conceptual/EventOverview/EventOverview.pdf
 */
-(void)updateTrackingRect
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (gdk_window);
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (private->impl);
  NSRect rect;

  if (trackingRect)
    {
      [self removeTrackingRect:trackingRect];
      trackingRect = 0;
    }

  if (!impl->toplevel)
    return;

  /* Note, if we want to set assumeInside we can use:
   * NSPointInRect ([[self window] convertScreenToBase:[NSEvent mouseLocation]], rect)
   */

  rect = [self bounds];
  trackingRect = [self addTrackingRect:rect
                                 owner:self
                              userData:nil
                          assumeInside:NO];
}

-(void)viewDidMoveToWindow
{
  if (![self window]) /* We are destroyed already */
      return;

  [self updateTrackingRect];
}

-(void)viewWillMoveToWindow:(NSWindow *)newWindow
{
  if (newWindow == nil && trackingRect)
    {
      [self removeTrackingRect:trackingRect];
      trackingRect = 0;
    }
}

-(void)setFrame:(NSRect)frame
{
  [super setFrame:frame];

  if ([self window])
    [self updateTrackingRect];
}

@end
