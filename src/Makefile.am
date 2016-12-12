#
# Copyright (c) 2016 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

localedir = $(datadir)/locale
introspectiondir = $(datadir)/dbus-1/interfaces

AM_CFLAGS = -D_REENTRANT=1 $(EXTRA_CFLAGS) -DLOCALEDIR=\"$(localedir)\" \
	-DINTROSPECTIONDIR=\"$(introspectiondir)\" -DSYSCONFDIR=\"$(sysconfdir)\"\
	-I$(top_srcdir)/lib ${DBUS_CFLAGS} ${GLIB_CFLAGS}

CLEANFILES = *~

noinst_HEADERS = rebootmgr.h log_msg.h etcd_handler.h
sbin_PROGRAMS = rebootmgrd rebootmgrctl

rebootmgrd_SOURCES = rebootmgrd.c log_msg.c etcd_handler.c
rebootmgrd_LDADD = -L../lib -lcalendarspec @DBUS_LIBS@ @GLIB_LIBS@
rebootmgrctl_LDADD = @DBUS_LIBS@ @GLIB_LIBS@
