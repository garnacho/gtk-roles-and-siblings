#!/usr/bin/perl -w

print <<EOF;
/* Generated by makegdkpixbufalias.pl */

#ifndef DISABLE_VISIBILITY

#include <glib.h>

#ifdef G_HAVE_GNUC_VISIBILITY

#ifdef  GDK_PIXBUF_DISABLE_DEPRECATED
#define WAS_NO_DEPR
#endif
#undef  GDK_PIXBUF_DISABLE_DEPRECATED

#ifdef  G_DISABLE_DEPRECATED
#define WAS_NO_G_DEPR
#endif
#undef  G_DISABLE_DEPRECATED

#include "gdk-pixbuf.h"
#include "gdk-pixdata.h"

EOF

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

  chop;
  my $str = $_;
  my @words;
  my $attributes = "";

  @words = split(/ /, $str);
  $str = shift(@words);
  chomp($str);
  my $alias = "IA__".$str;
  
  # Drop any Win32 specific .def file syntax,  but keep attributes
  foreach $word (@words) {
      $attributes = "$attributes $word" unless $word eq "PRIVATE";
  }
  
  print <<EOF
extern __typeof ($str) $alias __attribute((visibility("hidden")))$attributes;
extern __typeof ($str) $str __attribute((alias("$alias"), visibility("default")));
\#define $str $alias

EOF
}

print <<EOF;

#ifdef  WAS_NO_DEPR
#define GDK_PIXBUF_DISABLE_DEPRECATED
#undef  WAS_NO_DEPR
#endif

#ifdef  WAS_NO_G_DEPR
#define G_DISABLE_DEPRECATED
#undef  WAS_NO_G_DEPR
#endif

#endif /* G_HAVE_GNUC_VISIBILITY */

#endif /* DISABLE_VISIBILITY */
EOF


