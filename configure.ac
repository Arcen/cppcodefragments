#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([cppcodefragments], [1.0], [asakura@kbd.biglobe.ne.jp])
AC_CONFIG_SRCDIR([include/json.h])
AC_CONFIG_HEADERS([config.h])
LT_INIT
LT_CONFIG_LTDL_DIR([libltdl])
LTDL_INIT
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_CHECK_FUNCS([pow])

AC_CONFIG_FILES([Makefile
                 source/Makefile
                 test/Makefile])
AC_OUTPUT
