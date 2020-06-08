dnl Process this file with autoconf to produce a configure script.
AC_INIT(rebootmgr, 1.2)
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/rebootmgrd.c])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])
AC_PREFIX_DEFAULT(/usr)
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

dnl Some hacks...
test "${prefix}" = "NONE" && prefix="/usr"
if test ${prefix} = '/usr'
then
dnl If we use /usr as prefix, use /etc for ypserv.conf
        if test ${sysconfdir} = '${prefix}/etc'
        then
           sysconfdir="/etc"
        fi
dnl If we use /usr as prefix, use /usr/share/man for manual pages
        if test ${mandir} = '${prefix}/man'
        then
           mandir='${prefix}/share/man'
        fi
fi

if test "${exec_prefix}" = "NONE"
then
  TMPFILESDIR=${prefix}/lib/tmpfiles.d
  SYSTEMDDIR=${prefix}/lib/systemd/system
else
  TMPFILESDIR=${exec_prefix}/lib/tmpfiles.d
  SYSTEMDDIR=${exec_prefix}/lib/systemd/system
fi
AC_SUBST(TMPFILESDIR)
AC_SUBST(SYSTEMDDIR)

dnl Checks for programs.
AC_GNU_SOURCE
AC_PROG_CC
AC_ISC_POSIX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_LN_S

dnl enable additional compiler checks
AC_ARG_ENABLE([compiler-checks],
    AS_HELP_STRING([--enable-compiler-checks],[enables additional compiler checks, not for production]))

if test x"$enable_compiler_checks" = x"yes" ; then
   CFLAGS_CHECKS="-Wall -O2 -fsanitize=address -fno-omit-frame-pointer -g"
   LDFLAGS_CHECKS="-fsanitize=address"
fi

dnl enable additional compiler warnings
AC_ARG_ENABLE([compiler-warnings],
    AS_HELP_STRING([--enable-compiler-warnings],[enables additional compiler warnings]))

if test x"$enable_compiler_warnings" = x"yes" ; then
   if test x"$enable_compiler_checks" = x"yes" ; then
      AC_MSG_ERROR([Conflicting options: --enable-compiler-warnings cannot be used together with --eanble-compiler-checks])
   fi
   CFLAGS_WARNINGS="-O2 -g -Werror -W -Wall -DXTSTRINGDEFINES -D_FORTIFY_SOURCE=2 -fstack-protector-strong -funwind-tables -fasynchronous-unwind-tables -fstack-clash-protection -Werror=return-type -flto=8 -Wbad-function-cast -Wcast-align -Wcast-qual -Winline -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wshadow -Wstrict-prototypes -Wundef"
   LDFLAGS_WARNINGS=""
fi

AC_SUBST(CFLAGS_CHECKS)
AC_SUBST(LDFLAGS_CHECKS)
AC_SUBST(CFLAGS_WARNINGS)
AC_SUBST(LDFLAGS_WARNINGS)


dnl Check for gettid wrapper
AC_CHECK_FUNC(gettid, [AC_DEFINE(HAVE_GETTID, 1, [Define to 1 if gettid() is available])])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

PKG_CHECK_MODULES(DBUS, dbus-1 >= 0.60)
PKG_CHECK_MODULES(ECONF, libeconf)

AC_CHECK_LIB([cetcd],[cetcd_client_init],[ETCD_LIBS="-lcetcd"],[ETCD_LIBS=""])
if test x"" != x"${ETCD_LIBS}"
then
	AC_DEFINE(USE_ETCD, 1, [Use etcd for locking])
	AC_SUBST(ETCD_LIBS)
	PKG_CHECK_MODULES(JSON, json-c >= 0.13)
	AC_SUBST(JSON_CFLAGS)
	AC_SUBST(JSON_LIBS)
fi

dnl
dnl Check for xsltproc
dnl
enable_man=yes
AC_PATH_PROG([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
     enable_man=no
fi
AC_PATH_PROG([XMLLINT], [xmllint],[/bin/true])
dnl check for DocBook DTD and stylesheets in the local catalog.
JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.4//EN],
                [DocBook XML DTD V4.4], [], enable_man=no)
JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl],
                [DocBook XSL Stylesheets], [], enable_man=no)
AM_CONDITIONAL(ENABLE_REGENERATE_MAN, test x$enable_man != xno)

AC_OUTPUT([Makefile lib/Makefile src/Makefile man/Makefile dbus/Makefile \
	systemd/Makefile etc/Makefile tests/Makefile])
