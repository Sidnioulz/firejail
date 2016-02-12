AC_PREREQ([2.68])
AC_INIT(firejail, 0.9.29-fork, sidnioulz@gmail.com, , http://github.com/Sidnioulz/firejail)
AC_CONFIG_SRCDIR([src/firejail/main.c])
#AC_CONFIG_HEADERS([config.h])


AC_PROG_CC
#AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB

HAVE_SECCOMP=""
AC_ARG_ENABLE([seccomp],
    AS_HELP_STRING([--disable-seccomp], [Disable seccomp]))
AS_IF([test "x$enable_seccomp" != "xno"], [
	HAVE_SECCOMP="-DHAVE_SECCOMP"
	AC_SUBST(HAVE_SECCOMP)
])

HAVE_CHROOT=""
AC_ARG_ENABLE([chroot],
    AS_HELP_STRING([--disable-chroot], [Disable chroot]))
AS_IF([test "x$enable_chroot" != "xno"], [
	HAVE_CHROOT="-DHAVE_CHROOT"
	AC_SUBST(HAVE_CHROOT)
])

HAVE_BIND=""
AC_ARG_ENABLE([bind],
    AS_HELP_STRING([--disable-bind], [Disable bind]))
AS_IF([test "x$enable_bind" != "xno"], [
	HAVE_BIND="-DHAVE_BIND"
	AC_SUBST(HAVE_BIND)
])

HAVE_FATAL_WARNINGS=""
AC_ARG_ENABLE([fatal_warnings],
    AS_HELP_STRING([--enable-fatal-warnings], [-W -Wall -Werror]))
AS_IF([test "x$enable_fatal_warnings" = "xyes"], [
	HAVE_FATAL_WARNINGS="-W -Wall -Werror"
	AC_SUBST(HAVE_FATAL_WARNINGS)
])

# checking pthread library
AC_CHECK_LIB([pthread], [main], [], AC_MSG_ERROR([*** POSIX thread support not installed ***]))
AC_CHECK_HEADER(pthread.h,,AC_MSG_ERROR([*** POSIX thread support not installed ***]))
AC_CHECK_HEADER([linux/seccomp.h], HAVE_SECCOMP_H="-DHAVE_SECCOMP_H", HAVE_SECCOMP_H="")
AC_SUBST(HAVE_SECCOMP_H)

# checking for X11 for fireexecd
PKG_CHECK_MODULES(X11, [
	x11 >= 0.1
])

AM_CONDITIONAL([X11_IN_USE], [test "x$X11_LIBS" != x])
AM_COND_IF([X11_IN_USE],
           [HAVE_X11="-DHAVE_X11"],
           [HAVE_X11=""])

AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)
AC_SUBST(HAVE_X11)

# checking for libmount for OverlayFS
PKG_CHECK_MODULES(MOUNT, [
	mount >= 2.25
])
AC_SUBST(MOUNT_CFLAGS)
AC_SUBST(MOUNT_LIBS)

# checking for GTK+ for fireexecd
PKG_CHECK_MODULES(GTK, [
	glib-2.0 >= 2.40
	gio-2.0 >= 2.40
	gtk+-3.0 >= 3.10
])

AM_CONDITIONAL([GTK_IN_USE], [test "x$GTK_CFLAGS" != x])
AM_COND_IF([GTK_IN_USE],
           [HAVE_GTK="-DHAVE_GTK"],
           [HAVE_GTK=""])

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)
AC_SUBST(HAVE_GTK)

AC_OUTPUT(Makefile data/libexechelperutils.pc src/lib/Makefile src/firejail/Makefile src/firemon/Makefile src/fireexecd/Makefile src/libtrace/Makefile src/libexechelper/Makefile src/ftee/Makefile src/include/exechelper.h)

echo
echo "Configuration options:"
echo "   prefix: $prefix"
echo "   seccomp: $HAVE_SECCOMP"
echo "   <linux/seccomp.h>: $HAVE_SECCOMP_H"
echo "   chroot: $HAVE_CHROOT"
echo "   bind: $HAVE_BIND"
echo "   X11: $HAVE_X11"
echo "   GTK+: $HAVE_GTK"
echo "   fatal warnings: $HAVE_FATAL_WARNINGS"
echo
