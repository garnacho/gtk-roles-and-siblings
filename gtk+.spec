# Note that this is NOT a relocatable package
%define ver      1.1.8
%define rel      SNAP
%define prefix   /usr

Summary: The Gimp Toolkit
Name: gtk+
Version: %ver
Release: %rel
Copyright: LGPL
Group: X11/Libraries
Source: ftp://ftp.gimp.org/pub/gtk/v1.1/gtk+-%{ver}.tar.gz
BuildRoot: /tmp/gtk-root
Obsoletes: gtk
Packager: Marc Ewing <marc@redhat.com>
URL: http://www.gtk.org
Prereq: /sbin/install-info
Requires: glib
Docdir: %{prefix}/doc

%description
The X libraries originally written for the GIMP, which are now used by
several other programs as well.

%package devel
Summary: GIMP Toolkit and GIMP Drawing Kit
Group: X11/Libraries
Requires: gtk+
Obsoletes: gtk-devel
PreReq: /sbin/install-info

%description devel
Static libraries and header files for the GIMP's X libraries, which are
available as public libraries.  GLIB includes generally useful data
structures, GDK is a drawing toolkit which provides a thin layer over
Xlib to help automate things like dealing with different color depths,
and GTK is a widget set for creating user interfaces.

%changelog

* Sun Oct 25 1998 Shawn T. Amundson <amundson@gtk.org>

- Fixed Source: to point to v1.1 

* Tue Aug 04 1998 Michael Fulbright <msf@redhat.com>

- change %postun to %preun

* Mon Jun 27 1998 Shawn T. Amundson

- Changed version to 1.1.0

* Thu Jun 11 1998 Dick Porter <dick@cymru.net>

- Removed glib, since it is its own module now

* Mon Apr 13 1998 Marc Ewing <marc@redhat.com>

- Split out glib package

* Tue Apr  8 1998 Shawn T. Amundson <amundson@gtk.org>

- Changed version to 1.0.0

* Tue Apr  7 1998 Owen Taylor <otaylor@gtk.org>

- Changed version to 0.99.10

* Thu Mar 19 1998 Shawn T. Amundson <amundson@gimp.org>

- Changed version to 0.99.9
- Changed gtk home page to www.gtk.org

* Thu Mar 19 1998 Shawn T. Amundson <amundson@gimp.org>

- Changed version to 0.99.8

* Sun Mar 15 1998 Marc Ewing <marc@redhat.com>

- Added aclocal and bin stuff to file list.

- Added -k to the SMP make line.

- Added lib/glib to file list.

* Fri Mar 14 1998 Shawn T. Amundson <amundson@gimp.org>

- Changed version to 0.99.7

* Fri Mar 14 1998 Shawn T. Amundson <amundson@gimp.org>

- Updated ftp url and changed version to 0.99.6

* Thu Mar 12 1998 Marc Ewing <marc@redhat.com>

- Reworked to integrate into gtk+ source tree

- Truncated ChangeLog.  Previous Authors:
  Trond Eivind Glomsrod <teg@pvv.ntnu.no>
  Michael K. Johnson <johnsonm@redhat.com>
  Otto Hammersmith <otto@redhat.com>
  
%prep
%setup

%build
# Needed for snapshot releases.
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%prefix
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix
fi

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT%{prefix} install

gzip -9n $RPM_BUILD_ROOT%{prefix}/info/*

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%post devel
/sbin/install-info %{prefix}/info/gdk.info.gz %{prefix}/info/dir
/sbin/install-info %{prefix}/info/gtk.info.gz %{prefix}/info/dir

%preun devel
if [ $1 = 0 ]; then
    /sbin/install-info --delete %{prefix}/info/gdk.info.gz %{prefix}/info/dir
    /sbin/install-info --delete %{prefix}/info/gtk.info.gz %{prefix}/info/dir
fi

%files
%defattr(-, root, root)

%doc AUTHORS COPYING ChangeLog NEWS README TODO
%{prefix}/lib/libgtk-1.1.so.*
%{prefix}/lib/libgdk-1.1.so.*

%files devel
%defattr(-, root, root)

%{prefix}/lib/lib*.so
%{prefix}/lib/*a
%{prefix}/include/*
%{prefix}/info/*
%{prefix}/share/aclocal/*
%{prefix}/bin/*
