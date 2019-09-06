#
# Copyright (c) 2016 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

noinst_LIBRARIES = libcalendarspec.a

AM_CFLAGS = -D_REENTRANT=1 $(CFLAGS_CHECKS) #$(EXTRA_CFLAGS)

CLEANFILES = *~ test-calendarspec.log test-parse-duration.log \
	test-suite.log test-calendarspec.trs test-parse-duration.trs

noinst_HEADERS = calendarspec.h time-util.h parse-duration.h

libcalendarspec_a_SOURCES = calendarspec.c time-util.c parse-duration.c
