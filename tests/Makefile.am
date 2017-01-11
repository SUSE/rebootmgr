#
# Copyright (c) 2017 Thorsten Kukuk, Germany
#
# Author: Thorsten Kukuk <kukuk@suse.de>
#

AM_CFLAGS = -D_REENTRANT=1 -I$(top_srcdir)/lib -I$(top_srcdir)/src

CLEANFILES = *~ test-calendarspec.log test-parse-duration.log \
	test-suite.log test-calendarspec.trs test-parse-duration.trs \
	test-etcd.log test-etcd.trs test-json.log test-json.trs

noinst_PROGRAMS = test-calendarspec test-parse-duration test-json test-etcd
test_calendarspec_LDADD = ../lib/libcalendarspec.a
test_parse_duration_LDADD = ../lib/libcalendarspec.a
test_json_LDADD = ../src/lock-json.o ../src/log_msg.o @JSON_LIBS@
test_etcd_LDADD = ../src/lock-json.o ../src/lock-etcd.o ../src/log_msg.o @JSON_LIBS@ -lcetcd

TESTS = test-calendarspec test-parse-duration test-json test-etcd
EXTRA_DIST = ${TESTS}
