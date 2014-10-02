AC_INIT([lib_pugixmle],[1.4],[support@ridgerun.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

LT_INIT
AC_ENABLE_SHARED
AC_DISABLE_STATIC
AC_PROG_CXX
AC_PROG_LIBTOOL
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile libpugixml.pc:libpugixml.pc.in])

# Additional arguments (and substitutions)
# Allow the pkg-config directory to be set
#AC_ARG_WITH(pkgconfigdir,
#        AC_HELP_STRING([--with-pkgconfigdir],
#        [Use the specified pkgconfig dir (default is libdir/pkgconfig)]),
#        [pkgconfigdir=${withval}],
#        [pkgconfigdir='${libdir}/pkgconfig'])


dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

AC_OUTPUT




