#
# Copyright (c) 2016 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

noinst_LIBRARIES = libcalendarspec.a

AM_CFLAGS = -D_REENTRANT=1 \
	-DLOCALEDIR=\"$(localedir)\" ${DBUS_CFLAGS} ${GLIB_CFLAGS}

CLEANFILES = *~

noinst_HEADERS = calendarspec.h time-util.h

libcalendarspec_a_SOURCES = calendarspec.c time-util.c

noinst_PROGRAMS = test-calendarspec
test_calendarspec_LDADD = libcalendarspec.a
