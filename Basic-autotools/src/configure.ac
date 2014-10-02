AC_INIT([gst_remote],[1.0],[support@ridgerun.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AC_PROG_LIBTOOL
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AM_PROG_VALAC([0.7.7])

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl Now we're ready to ask for gobject
PKG_CHECK_MODULES(GOBJECT, gobject-2.0 >= 2.26.1, HAVE_GOBJECT=yes, HAVE_GOBJECT=no)

dnl Give error and exit if we don't have directfb
if test "x$HAVE_GOBJECT" = "xno"; then
  AC_MSG_ERROR([you need gobject development packages installed >= 2.26.1 !])
fi

dnl append GOBJECT_ERROR cflags to GOBJECT_CFLAGS
GOBJECT_CFLAGS="$GOBJECT_CFLAGS $GOBJECT_ERROR"

dnl make GOBJECT_CFLAGS and GOBJECT_LIBS available
AC_SUBST(GOBJECT_CFLAGS)
AC_SUBST(GOBJECT_LIBS)

dnl Now we're ready to ask for glib
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.26.1, HAVE_GLIB=yes, HAVE_GLIB=no)

dnl Give error and exit if we don't have directfb
if test "x$HAVE_GLIB" = "xno"; then
  AC_MSG_ERROR([you need glib development packages installed >= 2.26.1 !])
fi

dnl append GOBJECT_ERROR cflags to GOBJECT_CFLAGS
GLIB_CFLAGS="$GLIB_CFLAGS $GLIB_ERROR"

dnl make GOBJECT_CFLAGS and GOBJECT_LIBS available
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)


dnl Now we're ready to ask for gio
PKG_CHECK_MODULES(GIO, gio-2.0 >= 2.26.1, HAVE_GIO=yes, HAVE_GIO=no)

dnl Give error and exit if we don't have directfb
if test "x$HAVE_GIO" = "xno"; then
  AC_MSG_ERROR([you need gio development packages installed >= 2.26.1 !])
fi

dnl append GIO_ERROR cflags to GIO_CFLAGS
GIO_CFLAGS="$GIO_CFLAGS $GIO_ERROR"

dnl make GIO_CFLAGS and GIO_LIBS available
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

dnl Now we're ready to ask for gstreamer
PKG_CHECK_MODULES(GSTREAMER, gstreamer-0.10 >= 0.10.23 , HAVE_GSTREAMER=yes, HAVE_GSTREAMER=no)

dnl Give error and exit if we don't have directfb
if test "x$HAVE_GSTREAMER" = "xno"; then
  AC_MSG_ERROR([you need gstreamer development packages installed >= 0.10.23 !])
fi

dnl append GSTREAMER_ERROR cflags to GSTREAMER_CFLAGS
GSTREAMER_CFLAGS="$GSTREAMER_CFLAGS $GSTREAMER_ERROR"

dnl make GSTREAMER_CFLAGS and GSTREAMER_LIBS available
AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

dnl Now we're ready to ask for gst-rtsp-server
PKG_CHECK_MODULES(GST_RTSP_SERVER, gst-rtsp-server-0.10, HAVE_GST_RTSP_SERVER=yes, HAVE_GST_RTSP_SERVER=no)
if test "x$HAVE_GST_RTSP_SERVER" = "xno"; then
  AC_MSG_ERROR([you need gst-rtsp-server-0.10 development packages installed!])
fi
dnl make GST_RTSP_SERVER_CFLAGS and GST_RTSP_SERVER_LIBS available
AC_SUBST(GST_RTSP_SERVER_CFLAGS)
AC_SUBST(GST_RTSP_SERVER_LIBS)

AC_OUTPUT




