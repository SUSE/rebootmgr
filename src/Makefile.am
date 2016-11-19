#
# Copyright (c) 2016 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

localedir = $(datadir)/locale

AM_CFLAGS = -D_REENTRANT=1 $(EXTRA_CFLAGS) -DLOCALEDIR=\"$(localedir)\" \
	-I$(top_srcdir)/lib ${DBUS_CFLAGS} ${GLIB_CFLAGS}

CLEANFILES = *~

noinst_HEADERS = rebootmgr.h log_msg.h
sbin_PROGRAMS = rebootmgrd rebootmgrctl

rebootmgrd_SOURCES = rebootmgrd.c log_msg.c
rebootmgrd_LDADD = -L../lib -lcalendarspec @DBUS_LIBS@ @GLIB_LIBS@
rebootmgrctl_LDADD = @DBUS_LIBS@ @GLIB_LIBS@
