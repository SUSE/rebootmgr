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
                  'c_std=gnu17',
		  'warning_level=2',],
  license : ['GPL-2.0-only', 'LGPL-2.1-or-later'],
  version : '3.3',
)

fs = import('fs')
pkg = import('pkgconfig')

cc = meson.get_compiler('c')

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
                  '-Wformat-security',
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
libexecdir = join_paths(prefixdir, get_option('libexecdir'))
sysconfdir = join_paths('/', get_option('sysconfdir'))
mandir = join_paths(prefixdir, get_option('mandir'))
datadir = join_paths(prefixdir, get_option('datadir'))
configdir = join_paths(prefixdir, get_option('datadir'), meson.project_name())

inc = include_directories(['lib/calendarspec', 'lib/common', 'src', '.'])

subdir('lib')

libeconf = dependency('libeconf', version : '>=0.7.5')
libsystemd = dependency('libsystemd', version : '>=257')

rebootmgrctl_c = ['src/rebootmgrctl.c']
rebootmgrd_c = ['src/rebootmgrd.c', 'src/varlink-org.openSUSE.rebootmgr.c']

executable('rebootmgrctl',
           rebootmgrctl_c,
           include_directories : inc,
           dependencies : [libeconf, libsystemd],
           link_with : [libcommon_a, libcalendarspec_a],
           install : true,
	   install_dir: bindir)

executable('rebootmgrd',
           rebootmgrd_c,
           include_directories : inc,
           dependencies : [libeconf, libsystemd],
	   link_with : [libcommon_a, libcalendarspec_a],
           install : true,
	   install_dir: libexecdir)

subdir('etc')
subdir('systemd')
subdir('man')
subdir('shell-completion/bash')

# Unit tests
subdir('tests')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('LOCALEDIR', localedir)
conf.set_quoted('SYSCONFDIR', sysconfdir)
conf.set_quoted('DATADIR', datadir)
conf.set_quoted('CONFIGDIR', configdir)

config_h = configure_file(
  output : 'config.h',
  configuration : conf)
