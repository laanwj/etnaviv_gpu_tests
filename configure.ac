AC_PREREQ([2.60])

AC_INIT([etnaviv_gpu_tests], [0.1],
    [https://github.com/etnaviv])
AC_CONFIG_AUX_DIR([bin])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([foreign]) # tar-ustar dist-xz subdir-objects])

dnl Check for progs
AC_PROG_CPP
AC_PROG_CC
AC_PROG_RANLIB
AC_CHECK_PROGS([PYTHON3], [python3])
AC_PROG_SED

# Check for dependenicies
LIBDRM_REQUIRED=2.4.64
LIBDRM_ETNAVIV_REQUIRED=2.4.71

PKG_CHECK_MODULES([LIBDRM], [libdrm >= $LIBDRM_REQUIRED],
                  [have_libdrm=yes], [have_libdrm=no])
if test "x$have_libdrm" != xyes; then
   AC_MSG_ERROR([$1 requires libdrm >= $LIBDRM_REQUIRED])
fi
PKG_CHECK_MODULES([LIBDRM_ETNAVIV], [libdrm_etnaviv >= $LIBDRM_ETNAVIV_REQUIRED],
                  [have_libdrm_etnaviv=yes], [have_libdrm_etnaviv=no])
if test "x$have_libdrm" != xyes; then
   AC_MSG_ERROR([$1 requires libdrm_etnaviv >= $LIBDRM_ETNAVIV_REQUIRED])
fi

# Flags
CFLAGS="$CFLAGS -Wall -Wextra -O2 -g"

# Generate makefiles
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

dnl
dnl Output some configuration info for the user
dnl
echo ""
echo "        prefix:          $prefix"
echo "        exec_prefix:     $exec_prefix"
echo "        libdir:          $libdir"
echo "        includedir:      $includedir"
