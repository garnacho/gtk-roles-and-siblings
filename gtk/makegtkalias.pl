#!/usr/bin/perl -w

print <<EOF;
/* Generated by makegtkalias.pl */

#include <glib.h>

#ifdef G_HAVE_GNUC_VISIBILITY

#ifdef  GTK_ENABLE_BROKEN
#define WAS_BROKEN
#endif
#define GTK_ENABLE_BROKEN

#ifdef GTK_TEXT_USE_INTERNAL_UNSUPPORTED_API
#define WAS_UNSUPPORTED_TEXT_API
#endif
#define GTK_TEXT_USE_INTERNAL_UNSUPPORTED_API

#ifdef  GTK_DISABLE_DEPRECATED
#define WAS_NO_DEPR
#endif
#undef  GTK_DISABLE_DEPRECATED

#ifdef  G_DISABLE_DEPRECATED
#define WAS_NO_G_DEPR
#endif
#undef  G_DISABLE_DEPRECATED

#include "gtk.h"

#include "gtkfilesystem.h"
#ifdef G_OS_UNIX
#include "gtkfilesystemunix.h"
#endif
#ifdef G_OS_WIN32
#include "gtkfilesystemwin32.h"
#endif
#include "gtkhsv.h"
#include "gtkpathbar.h"
#include "gtktextdisplay.h"
#include "gtktextlayout.h"
#include "gtktextsegment.h"
#include "gtktexttypes.h"
#include "gtkthemes.h"
#include "gtkwindow-decorate.h"

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
  my $alias = "IA__".$str;
  
  print <<EOF
extern __typeof ($str) $alias __attribute((visibility("hidden")));
extern __typeof ($str) $str __attribute((alias("$alias"), visibility("default")));
\#define $str $alias

EOF
}

print <<EOF;

#ifndef WAS_BROKEN
#undef  GTK_ENABLE_BROKEN
#else
#undef  WAS_BROKEN
#endif

#ifndef WAS_UNSUPPORTED_TEXT_API
#undef GTK_TEXT_USE_INTERNAL_UNSUPPORTED_API
#else
#undef WAS_UNSUPPORTED_TEXT_API
#endif

#ifdef  WAS_NO_DEPR
#define GTK_DISABLE_DEPRECATED
#undef  WAS_NO_DEPR
#endif

#ifdef  WAS_NO_G_DEPR
#define G_DISABLE_DEPRECATED
#undef  WAS_NO_G_DEPR
#endif

#endif /* G_HAVE_GNUC_VISIBILITY */
EOF


