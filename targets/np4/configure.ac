#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([PI-np4], [0.1], [craig_stevens@dell.com])
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_CXX
LT_INIT

AC_CONFIG_MACRO_DIR([m4])

AC_LANG_PUSH(C)
AC_LANG_PUSH(C++)

# check for pthreads
# TODO(craigs): not sure why this errors out on autogen.sh
# comment out temporarily
#AX_PTHREAD([], [AC_MSG_ERROR([Missing pthread library])])

AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

# To simplify usage, we will update PATH, CPPFLAGS,.. to include the 'prefix'
# ones
adl_RECURSIVE_EVAL([$bindir], [BIN_DIR])
adl_RECURSIVE_EVAL([$includedir], [INCLUDE_DIR])
adl_RECURSIVE_EVAL([$libdir], [LIB_DIR])

AC_ARG_ENABLE([Werror],
    AS_HELP_STRING([--enable-Werror], [Make all compiler warnings fatal]),
    [enable_Werror="$enableval"], [enable_Werror=no])

AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_SIZE_T

EXTRA_CXXFLAGS="-Wall -Wextra"
AS_IF([test "$enable_Werror" = "yes"], [EXTRA_CXXFLAGS="$EXTRA_CXXFLAGS -Werror"])

AC_SUBST([AM_CPPFLAGS], ["-isystem$INCLUDE_DIR"])
AC_SUBST([AM_LDFLAGS], ["-L$LIB_DIR"])
AC_SUBST([AM_CXXFLAGS], ["$PTHREAD_CFLAGS $EXTRA_CXXFLAGS"])
AC_SUBST([AM_CFLAGS], ["$PTHREAD_CFLAGS"])

# Generate makefiles
AC_CONFIG_FILES([Makefile])

AX_PREFIX_CONFIG_H()

AC_OUTPUT
