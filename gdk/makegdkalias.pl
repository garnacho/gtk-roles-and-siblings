#!/usr/bin/perl -w

print <<EOF;
/* Generated by makegdkalias.pl */

#include <glib.h>

#ifdef G_HAVE_GNUC_VISIBILITY

#ifdef  GDK_ENABLE_BROKEN
#define WAS_BROKEN
#endif
#define GDK_ENABLE_BROKEN

#ifdef  GDK_MULTIHEAD_SAFE
#define WAS_MULTIHEAD
#endif
#undef GDK_MULTIHEAD_SAVE

#ifdef  GDK_DISABLE_DEPRECATED
#define WAS_NO_DEPR
#endif
#undef  GDK_DISABLE_DEPRECATED

#ifdef  G_DISABLE_DEPRECATED
#define WAS_NO_G_DEPR
#endif
#undef  G_DISABLE_DEPRECATED

#include "gdk.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif
#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#endif

EOF

my $in_comment = 0;
my $in_skipped_section = 0;

while (<>) {

  # ignore empty lines
  next if /^\s*$/;

  # skip comments
  if ($_ =~ /^\s*\/\*/)
  {
      $in_comment = 1;
  }
  
  if ($in_comment)
  {
      if ($_ =~  /\*\/\s$/)
      {
	  $in_comment = 0;
      }
      
      next;
  }

  # handle ifdefs
  if ($_ =~ /^\#endif/)
  {
      if (!$in_skipped_section)
      {
	  print $_;
      }

      $in_skipped_section = 0;

      next;
  }

  if ($_ =~ /^\#ifdef\s+INCLUDE_VARIABLES/)
  {
      $in_skipped_section = 1;
  }

  if ($in_skipped_section)
  {
      next;
  }

  if ($_ =~ /^\#ifdef\s+G/)
  {
      print $_;
      
      next;
  }

  my $str = $_;
  chomp($str);
  my $alias = $str."__internal_alias";
 
  print <<EOF
extern __typeof ($str) $alias __attribute((visibility("hidden")));
extern __typeof ($str) $str __attribute((alias("$alias"), visibility("default")));
\#define $str $alias

EOF
}

print <<EOF;

#ifndef WAS_BROKEN
#undef  GDK_ENABLE_BROKEN
#else
#undef  WAS_BROKEN
#endif

#ifdef  WAS_MULTIHEAD
#define GDK_MULTIHEAD_SAFE
#undef  WAS_MULTIHEAD
#endif

#ifdef  WAS_NO_DEPR
#define GDK_DISABLE_DEPRECATED
#undef  WAS_NO_DEPR
#endif

#ifdef  WAS_NO_G_DEPR
#define G_DISABLE_DEPRECATED
#undef  WAS_NO_G_DEPR
#endif

#endif /* G_HAVE_GNUC_VISIBILITY */
EOF

