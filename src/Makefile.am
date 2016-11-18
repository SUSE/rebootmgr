#
# Copyright (c) 2016 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

localedir = $(datadir)/locale

AM_CFLAGS = -D_REENTRANT=1 $(EXTRA_CFLAGS) \
	-DLOCALEDIR=\"$(localedir)\" ${DBUS_CFLAGS} ${GLIB_CFLAGS}
AM_LDFLAGS = @DBUS_LIBS@ @GLIB_LIBS@

CLEANFILES = *~

noinst_HEADERS = rebootmgr.h log_msg.h
sbin_PROGRAMS = rebootmgrd rebootmgrctl

rebootmgrd_SOURCES = rebootmgrd.c log_msg.c
