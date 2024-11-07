project(
  'rebootmgr',
  'c',
  meson_version : '>= 0.61.0',
  default_options : [
                  'prefix=/usr',
                  'sysconfdir=/etc',
                  'localstatedir=/var',
                  'buildtype=debugoptimized',
  		  'default_library=static',
		  'b_pie=true',
                  'b_lto=true',
		  'warning_level=3',],
  license : ['GPL-2.0-only', 'LGPL-2.1-or-later'],
  version : '2.6',
)

fs = import('fs')
pkg = import('pkgconfig')

cc = meson.get_compiler('c')
inc = include_directories('lib')

add_project_arguments(['-D_GNU_SOURCE=1',
		       '-DXTSTRINGDEFINES',
		       '-D_FORTIFY_SOURCE=2',
                       '-D_FILE_OFFSET_BITS=64',
                       '-D_TIME_BITS=64'], language : 'c')

possible_cc_flags = [
		  '-fstack-protector-strong',
		  '-funwind-tables',
		  '-fasynchronous-unwind-tables',
		  '-fstack-clash-protection',
		  '-Werror=return-type',
		  '-Wbad-function-cast',
		  '-Wcast-align',
		  '-Wcast-qual',
                  '-Wformat-security',
		  '-Winline',
		  '-Wmissing-declarations',
		  '-Wmissing-prototypes',
		  '-Wnested-externs',
		  '-Wshadow',
		  '-Wstrict-prototypes',
		  '-Wundef',
		  ]
add_project_arguments(cc.get_supported_arguments(possible_cc_flags), language : 'c')

prefixdir = get_option('prefix')
if not fs.is_absolute(prefixdir)
        error('Prefix is not absolute: "@0@"'.format(prefixdir))
endif
systemunitdir = join_paths(prefixdir, 'lib/systemd/system')
tmpfilesdir = join_paths(prefixdir, 'lib/tmpfiles.d')
localedir = join_paths(prefixdir, get_option('localedir'))
bindir = join_paths(prefixdir, get_option('bindir'))
sbindir = join_paths(prefixdir, get_option('sbindir'))
sysconfdir = join_paths('/', get_option('sysconfdir'))
mandir = join_paths(prefixdir, get_option('mandir'))
busconfigdir = join_paths(prefixdir, get_option('datadir'), 'dbus-1/system.d')
introspectiondir = join_paths(prefixdir, get_option('datadir'), 'dbus-1/interfaces')
configdir = join_paths(prefixdir, get_option('datadir'), meson.project_name())

libeconf = dependency('libeconf', version : '>=0.7.4')
libdbus_1 = dependency('dbus-1')

libcalendarspec_c = ['lib/calendarspec.c', 'lib/parse-duration.c',
		  'lib/time-util.c']

rebootmgrctl_c = ['src/rebootmgrctl.c', 'src/util.c', 'src/log_msg.c']
rebootmgrd_c = ['src/rebootmgrd.c', 'src/log_msg.c', 'src/config_file.c',
	     'src/util.c']

libcalendarspec_a = static_library(
  'libcalendarspec',
  libcalendarspec_c,
  install : false)

executable('rebootmgrctl',
           rebootmgrctl_c,
           include_directories : inc,
           dependencies : [libeconf, libdbus_1],
	   link_with : [libcalendarspec_a],
           install : true,
	   install_dir: bindir)

executable('rebootmgrd',
           rebootmgrd_c,
           include_directories : inc,
           dependencies : [libeconf, libdbus_1],
	   link_with : [libcalendarspec_a],
           install : true,
	   install_dir: sbindir)

subdir('etc')
subdir('systemd')

feature_man = get_option('man')

prog_xmllint = find_program(
  'xmllint',
  required: feature_man,
  disabler: true,
)

enable_man = not feature_man.disabled()
if enable_man
  prog_xsltproc = find_program(
    'xsltproc',
    required: feature_man,
    disabler: true,
  )
  prog_xmllint = find_program(
    'xmllint',
    required: feature_man,
    disabler: true,
  )
  prog_xmlcatalog = find_program(
    'xmlcatalog',
    required: feature_man,
    disabler: true,
  )
  xml_catalog = get_option('xml-catalog')
  if xml_catalog == ''
    xml_catalog = '/etc/xml/catalog'
  endif

  docbook_rng = get_option('docbook-rng')
  have = run_command(
    [prog_xmlcatalog, '--noout', xml_catalog, docbook_rng],
    check: feature_man.enabled()
  ).returncode() == 0
  if not have
    docbook_rng = disabler()
  endif

  man_stylesheet = get_option('man-stylesheet')
  have = run_command(
    [prog_xmlcatalog, '--noout', xml_catalog, man_stylesheet],
    check: feature_man.enabled()
  ).returncode() == 0
  if not have
    man_stylesheet = disabler()
  endif
else
  prog_xsltproc = disabler()
  prog_xmllint = disabler()
  prog_xmlcatalog = disabler()
  docbook_rng = disabler()
  man_stylesheet = disabler()
endif

subdir('dbus')
subdir('man')

# Unit tests
subdir('tests')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('LOCALEDIR', localedir)
conf.set_quoted('SYSCONFDIR', sysconfdir)
conf.set_quoted('CONFIGDIR', configdir)
conf.set_quoted('INTROSPECTIONDIR', introspectiondir)

config_h = configure_file(
  output : 'config.h',
  configuration : conf)
