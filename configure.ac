AC_INIT([amloadsvf],[1.0],[bug-automake@gnu.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
  src/Makefile
])

AC_ARG_ENABLE([legacy-ioctl],
              [AS_HELP_STRING([--enable-legacy-ioctl],[Use legacy ioctl])])
AS_IF([test "x$enable_legacy_ioctl" = "xyes"],
      [AC_DEFINE([USE_LEGACY_IOCTL], [1], [Define to 1 if you use legacy ioctl])],
      [])

AC_OUTPUT
