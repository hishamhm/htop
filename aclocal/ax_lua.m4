# ===========================================================================
#          http://www.gnu.org/software/autoconf-archive/ax_lua.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_WITH_LUA
#   AX_LUA_VERSION (MIN-VERSION, [TOO-BIG-VERSION])
#   AX_LUA_HEADERS
#   AX_LUA_LIBS
#   AX_LUA_LIB_VERSION (MIN-VERSION, [TOO-BIG-VERSION])
#
# DESCRIPTION
#
#   Detect Lua interpreter, headers and libraries, optionally enforcing a
#   particular range of versions.
#
#   AX_WITH_LUA searches for Lua interpreter and defines LUA if found.
#
#   AX_LUA_VERSION checks that the version of Lua is at least MIN-VERSION
#   and less than TOO-BIG-VERSION, if given.
#
#   AX_LUA_HEADERS searches for Lua headers and defines HAVE_LUA_H and
#   HAVE_LUALIB_H if found, and defines LUA_INCLUDE to the preprocessor
#   flags needed, if any.
#
#   AX_LUA_LIBS searches for Lua libraries and defines LUA_LIB if found.
#
#   AX_LUA_LIB_VERSION checks that the Lua libraries' version is at least
#   MIN-VERSION, and less than TOO-BIG-VERSION, if given.
#
#   Versions are specified as three-digit integers whose first digit is the
#   major version and last two are the minor version (the same format as
#   LUA_VERSION_NUM in lua.h); e.g. 501 for Lua 5.1. The revision (e.g. the
#   "3" in "5.1.3") is ignored.
#
#   The following options are added by these macros:
#
#     --with-lua-prefix=DIR     Lua files are in DIR.
#     --with-lua-suffix=ARG     Lua binaries and library files are
#                               suffixed with ARG.
#
# LICENSE
#
#   Copyright (c) 2012 Hisham Muhammad <h@hisham.hm>
#   Copyright (c) 2009 Reuben Thomas <rrt@sc3d.org>
#   Copyright (c) 2009 Matthieu Moy <Matthieu.Moy@imag.fr>
#   Copyright (c) 2009 Tom Payne <twpayne@gmail.com>
#
#   This program is free software: you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation, either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 7

dnl Helper function to declare extra options
AC_DEFUN([_AX_LUA_OPTS],
  [AC_ARG_WITH([lua-prefix],
     [AS_HELP_STRING([--with-lua-prefix=DIR],
        [Lua files are in DIR])])
   AC_ARG_WITH([lua-suffix],
     [AS_HELP_STRING([--with-lua-suffix=ARG],
        [Lua binary and library files are suffixed with ARG])])])dnl

AC_DEFUN([AX_WITH_LUA],
  [_AX_LUA_OPTS
  if test "x$with_lua_prefix" = x; then
    lua_search_path="$PATH"
  else
    lua_search_path="$with_lua_prefix/bin"
  fi
  if test "x$LUA" = x; then
    AC_PATH_PROG([LUA], [lua$with_lua_suffix], [], [$lua_search_path])
  fi])dnl

dnl Helper function to parse minimum & maximum versions
AC_DEFUN([_AX_LUA_VERSIONS],
  [lua_min_version=$1
  lua_max_version=$2
  if test "x$lua_min_version" = x; then
    lua_min_version=0
  fi
  if test "x$lua_max_version" = x; then
    lua_max_version=1000
  fi])

AC_DEFUN([AX_LUA_VERSION],
  [_AX_LUA_OPTS
  AC_MSG_CHECKING([Lua version is in range $1 <= v < $2])
  _AX_LUA_VERSIONS($1, $2)
  if test "x$LUA" != x; then
    lua_text_version=$(LUA_INIT= $LUA -v 2>&1 | head -n 1 | cut -d' ' -f2)
    case $lua_text_version in
    5.3*)
      lua_version=503
      ;;
    5.2*)
      lua_version=502
      ;;
    5.1*)
      lua_version=501
      ;;
    5.0*)
      lua_version=500
      ;;
    4.0*)
      lua_version=400
      ;;
    *)
      lua_version=-1
      ;;
    esac
    if test $lua_version -ge "$lua_min_version" -a $lua_version -lt "$lua_max_version"; then
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT([no])
      AC_MSG_FAILURE([Lua version not in desired range.])
    fi
  else
    AC_MSG_RESULT([no])
    AC_MSG_FAILURE([Lua version not in desired range.])
  fi])dnl

AC_DEFUN([AX_LUA_HEADERS],
  [_AX_LUA_OPTS
  if test "x$with_lua_includes" != x; then
    LUA_INCLUDE="-I$with_lua_includes"
  elif test "x$with_lua_prefix" != x; then
    if test -e "$with_lua_prefix/include/lua5.3"; then
      LUA_INCLUDE="-I$with_lua_prefix/include/lua5.3"
    elif test -e "$with_lua_prefix/include/lua5.2"; then
      LUA_INCLUDE="-I$with_lua_prefix/include/lua5.2"
    elif test -e "$with_lua_prefix/include/lua5.1"; then
      LUA_INCLUDE="-I$with_lua_prefix/include/lua5.1"
    else
      LUA_INCLUDE="-I$with_lua_prefix/include"
    fi
  elif test -e "/usr/include/lua5.3"; then
    LUA_INCLUDE="-I/usr/include/lua5.3"
  elif test -e "/usr/include/lua5.2"; then
    LUA_INCLUDE="-I/usr/include/lua5.2"
  elif test -e "/usr/include/lua5.1"; then
    LUA_INCLUDE="-I/usr/include/lua5.1"
  fi
  LUA_OLD_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $LUA_INCLUDE"
  AC_CHECK_HEADERS([lua.h lualib.h])
  CPPFLAGS="$LUA_OLD_CPPFLAGS"])dnl

AC_DEFUN([AX_LUA_LIBS],
  [_AX_LUA_OPTS
  if test "x$with_lua_prefix" != x; then
    LUA_LIB="-L$with_lua_prefix/lib"
    LUA_LIBLUA="$with_lua_prefix/lib/liblua$with_lua_suffix.a"
  else
    # try to find static library
    for dir in `echo $LD_LIBRARY_PATH | tr ':' ' '` /usr /usr/local
    do
       LUA_LIBLUA="$dir/lib/liblua$with_lua_suffix.a"
       if test -e "$LUA_LIBLUA"
       then
          break
       fi
    done
  fi
  # if static library not found, use dynamic
  if ! test -e "$LUA_LIBLUA"; then
    LUA_LIBLUA="-llua$with_lua_suffix"
  fi
  AC_CHECK_LIB([m], [exp], [lua_extra_libs="$lua_extra_libs -lm"], [])
  AC_CHECK_LIB([dl], [dlopen], [lua_extra_libs="$lua_extra_libs -ldl"], [])
  AC_CHECK_LIB([lua$with_lua_suffix],
    [lua_error],
    [LUA_LIB="$LUA_LIB $LUA_LIBLUA $lua_extra_libs"],
    [],
    [$LUA_LIB $lua_extra_libs])])dnl

AC_DEFUN([AX_LUA_LIB_VERSION],
  [_AX_LUA_OPTS
  AC_MSG_CHECKING([liblua version is in range $1 <= v < $2])
  _AX_LUA_VERSIONS($1, $2)
  LUA_OLD_LIBS="$LIBS"
  LIBS="$LIBS $LUA_LIB"
  LUA_OLD_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $LUA_INCLUDE"
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <lua.h>
#include <stdlib.h>
#include <stdio.h>
int main()
{
  printf("(found %s, %d)... ", LUA_VERSION, LUA_VERSION_NUM);
  if (LUA_VERSION_NUM >= $lua_min_version && LUA_VERSION_NUM < $lua_max_version)
    exit(EXIT_SUCCESS);
  exit(EXIT_FAILURE);
}
]])],
  [AC_MSG_RESULT([yes])],
  [AC_MSG_RESULT([no])
  AC_MSG_FAILURE([Lua libraries version not in desired range])])
  LIBS="$LUA_OLD_LIBS"
  CPPFLAGS="$LUA_OLD_CPPFLAGS"])dnl

AC_DEFUN([AX_LUA_TRY_LIB_VERSION],
  [_AX_LUA_OPTS
  _AX_LUA_VERSIONS($1, $2)
  AC_CACHE_CHECK([if liblua version is in range $1 <= v < $2], [ax_cv_lua_try_lib_version], [
LUA_OLD_LIBS="$LIBS"
LUA_OLD_CPPFLAGS="$CPPFLAGS"
LIBS="$LIBS $LUA_LIB"
CPPFLAGS="$CPPFLAGS $LUA_INCLUDE"
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <lua.h>
#include <stdlib.h>
#include <stdio.h>
int main()
{
  printf("(found %s, %d)... ", LUA_VERSION, LUA_VERSION_NUM);
  if (LUA_VERSION_NUM >= $lua_min_version && LUA_VERSION_NUM < $lua_max_version)
    exit(EXIT_SUCCESS);
  exit(EXIT_FAILURE);
}
]])],
  [AC_MSG_RESULT([yes])
   $3
  ],
  [
  AC_MSG_RESULT([no])
   $4
  ])
LIBS="$LUA_OLD_LIBS"
CPPFLAGS="$LUA_OLD_CPPFLAGS"
  ])
])dnl
