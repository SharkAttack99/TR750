# Copyright (C) 2013 Cryptotronix, LLC.

# This file is part of Hashlet.

# Hashlet is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.

# Hashlet is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with Hashlet.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([hashlet], [1.1.0], [bugs@cryptotronix.com], [hashlet], [http://www.github.com/cryptotronix/hashlet/])
AC_PREREQ([2.59])
AM_INIT_AUTOMAKE([1.10 -Wall no-define])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CHECK_HEADERS([linux/i2c-dev.h sys/ioctl.h fcntl.h gcrypt.h])
AC_CHECK_HEADER(argp.h,,[AC_MSG_ERROR([argp_parse not found])])
AC_SEARCH_LIBS(argp_parse,argp,,[AC_MSG_ERROR([argp_parse not found])])
AC_PROG_CC
AM_PROG_CC_C_O
AC_CONFIG_FILES([Makefile doc/Makefile])
PKG_PROG_PKG_CONFIG
#Check for libs
have_gcrypt=no
AC_SEARCH_LIBS([gcry_md_open], [gcrypt], [have_gcrypt=yes])

if test "x${have_gcrypt}" = xno; then
   AC_MSG_ERROR([
   ----------------------------------------------------
   Unable to find gcrypt.h on this system.
   libgcrypt (libgcrypt11-dev) is required, please install
   and rebuild.
   ----------------------------------------------------])
fi

AM_PROG_LEX
AC_PROG_YACC
AC_PROG_LIBTOOL


AC_OUTPUT
