#
# Copyright (c) 2016-2019 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

localedir = $(datadir)/locale
introspectiondir = $(datadir)/dbus-1/interfaces

CLEANFILES = *~

AM_CFLAGS = -D_REENTRANT=1 $(EXTRA_CFLAGS) -DLOCALEDIR=\"$(localedir)\" \
	-DINTROSPECTIONDIR=\"$(introspectiondir)\" -DSYSCONFDIR=\"$(sysconfdir)\"\
	-I$(top_srcdir)/lib ${DBUS_CFLAGS} ${GLIB_CFLAGS} ${JSON_CFLAGS}

noinst_HEADERS = rebootmgr.h log_msg.h config_file.h util.h \
	lock-json.h lock-etcd.h
sbin_PROGRAMS = rebootmgrd rebootmgrctl

rebootmgrd_SOURCES = rebootmgrd.c log_msg.c config_file.c util.c \
		lock-json.c lock-etcd.c
rebootmgrd_LDADD = -L../lib -lcalendarspec ${DBUS_LIBS} ${GLIB_LIBS} ${JSON_LIBS} ${ETCD_LIBS}
rebootmgrctl_SOURCES = rebootmgrctl.c util.c lock-etcd.c lock-json.c log_msg.c
rebootmgrctl_LDADD = -L../lib -lcalendarspec ${DBUS_LIBS} ${JSON_LIBS} ${ETCD_LIBS}

