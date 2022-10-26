#
# Copyright (c) 2017 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

AM_CFLAGS = -D_REENTRANT=1 -I$(top_srcdir)/lib -I$(top_srcdir)/src \
	    ${CFLAGS_CHECKS} ${CFLAGS_WARNINGS}

CLEANFILES = *~ test-calendarspec.log test-parse-duration.log \
	test-suite.log test-calendarspec.trs test-parse-duration.trs

noinst_PROGRAMS = test-calendarspec test-parse-duration
test_calendarspec_LDADD = ../lib/libcalendarspec.a ${LDFLAGS_CHECKS}
test_parse_duration_LDADD = ../lib/libcalendarspec.a

TESTS = test-calendarspec test-parse-duration
EXTRA_DIST = ${TESTS}
