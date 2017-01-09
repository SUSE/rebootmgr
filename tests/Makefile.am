#
# Copyright (c) 2017 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

AM_CFLAGS = -D_REENTRANT=1 -I../lib -I../src #$(EXTRA_CFLAGS)

CLEANFILES = *~ test-calendarspec.log test-parse-duration.log \
	test-suite.log test-calendarspec.trs test-parse-duration.trs

noinst_PROGRAMS = test-calendarspec test-parse-duration test-json
test_calendarspec_LDADD = ../lib/libcalendarspec.a
test_parse_duration_LDADD = ../lib/libcalendarspec.a
test_json_LDADD = ../src/lock-json.o @JSON_LIBS@

TESTS = test-calendarspec test-parse-duration test-json
EXTRA_DIST = ${TESTS}
