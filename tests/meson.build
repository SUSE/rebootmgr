test_calendarspec_exe = executable('test-calendarspec', 'test-calendarspec.c', include_directories : inc, link_with: libcalendarspec_a)
test('test-calendarspec', test_calendarspec_exe)
test_parse_duration_exe = executable('test-parse-duration', 'test-parse-duration.c', include_directories : inc, link_with: libcalendarspec_a)
test('test-parse-duration', test_parse_duration_exe)

tst_mkdir_p_exe = executable('tst-mkdir_p', 'tst-mkdir_p.c',
  include_directories : inc, link_with: libcommon_a)
test('tst-mkdir_p', tst_mkdir_p_exe)
