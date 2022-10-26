#
# Copyright (c) 2016-2020 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

distconfdir = /usr/etc
localedir = $(datadir)/locale
introspectiondir = $(datadir)/dbus-1/interfaces

CLEANFILES = *~

AM_CFLAGS = -D_REENTRANT=1 -DLOCALEDIR=\"$(localedir)\" \
	-DINTROSPECTIONDIR=\"$(introspectiondir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" -DDISTCONFDIR=\"$(distconfdir)\" \
	-I$(top_srcdir)/lib ${DBUS_CFLAGS} ${JSON_CFLAGS} \
	${ECONF_CFLAGS} ${CFLAGS_WARNINGS} ${CFLAGS_CHECKS}

noinst_HEADERS = rebootmgr.h log_msg.h config_file.h util.h
sbin_PROGRAMS = rebootmgrd rebootmgrctl

rebootmgrd_SOURCES = rebootmgrd.c log_msg.c config_file.c util.c 
rebootmgrd_LDADD = -L../lib -lcalendarspec ${DBUS_LIBS} \
		${ECONF_LIBS} -lrt -lpthread ${LDFLAGS_CHECKS}
rebootmgrctl_SOURCES = rebootmgrctl.c util.c log_msg.c
rebootmgrctl_LDADD = -L../lib -lcalendarspec ${DBUS_LIBS} ${LDFLAGS_CHECKS}

