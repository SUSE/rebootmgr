dnl Process this file with autoconf to produce a configure script.
AC_INIT(rebootmgr, 0.11)
AM_INIT_AUTOMAKE
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

if test ${libexecdir} = '${exec_prefix}/libexec'
then
  libexecdir='${exec_prefix}/lib'
  if test "${exec_prefix}" = "NONE"
  then
    TMPFILESDIR=${prefix}/lib/tmpfiles.d
    SYSTEMDDIR=${prefix}/lib/systemd/system
  else
    TMPFILESDIR=${exec_prefix}/lib/tmpfiles.d
    SYSTEMDDIR=${exec_prefix}/lib/systemd/system
  fi
else
  TMPFILESDIR=${libexecdir}/tmpfiles.d
  SYSTEMDDIR=${libexecdir}/systemd/system
fi
AC_SUBST(TMPFILESDIR)
AC_SUBST(SYSTEMDDIR)




EXTRA_CFLAGS="-Werror -W -Wall -Wbad-function-cast -Wcast-align -Wcast-qual -DXTSTRINGDEFINES -Winline -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wshadow -Wstrict-prototypes -Wundef -D_FORTIFY_SOURCE=2"
AC_SUBST(EXTRA_CFLAGS)
dnl Checks for programs.
AC_GNU_SOURCE
AC_PROG_CC
AC_ISC_POSIX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_LN_S

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

PKG_CHECK_MODULES(DBUS, dbus-glib-1 >= 0.60)
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.40)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
PKG_CHECK_MODULES([JSON], [json-c])
AC_SUBST(JSON_CFLAGS)
AC_SUBST(JSON_LIBS)

AC_CHECK_LIB([cetcd],[cetcd_client_init],[], [
  AC_MSG_ERROR([unable to find the cetcd library])
])

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

dnl gettext support
AM_GNU_GETTEXT_VERSION
AM_GNU_GETTEXT([external])
AC_CHECK_FUNCS(dngettext)

AH_VERBATIM([__ZZENABLE_NLS],
[#ifdef ENABLE_NLS
#include <locale.h>
#include <libintl.h>
#define _(msgid) gettext(msgid)
#define N_(msgid) msgid
#else
#define _(msgid) (msgid)
#define N_(msgid) msgid
#endif /* ENABLE_NLS */])

AC_OUTPUT([Makefile lib/Makefile src/Makefile man/Makefile dbus/Makefile \
	systemd/Makefile etc/Makefile tests/Makefile])
