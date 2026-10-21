project(
  'metrics-exporter',
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
  license : ['GPL-2.0-or-later', 'LGPL-2.1-or-later'],
  version : '0.1',
)

prefixdir = get_option('prefix')
if not prefixdir.startswith('/')
        error('Prefix is not absolute: "@0@"'.format(prefixdir))
endif
libexecdir = join_paths(prefixdir, get_option('libexecdir'))
sysconfdir = join_paths('/', get_option('sysconfdir'))
datadir = join_paths(prefixdir, get_option('datadir'))
configdir = join_paths(prefixdir, get_option('datadir'), meson.project_name())

cc = meson.get_compiler('c')
pkg = import('pkgconfig')
inc = include_directories(['include', 'lib'])

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

conf = configuration_data()

libeconf = dependency('libeconf', version : '>=0.7.5')
libsystemd = dependency('libsystemd', version : '>=260')
libselinux = dependency('libselinux', required: get_option('selinux'))
if libselinux.found()
  conf.set('WITH_SELINUX', 1)
endif
libnm = dependency('libnm', required: get_option('libnm'))
if libnm.found()
  conf.set('WITH_NETWORK_MANAGER', 1)
endif
librpm = dependency('rpm', required: get_option('librpm'))
if librpm.found()
  conf.set('WITH_LIBRPM', 1)
endif

libcommon_c = files('lib/log_msg.c', 'lib/exec_cmd.c', 'lib/read_config.c')
libcommon = static_library(
  'common',
  libcommon_c,
  include_directories : inc,
  install : false
)

metrics_exporter_c = ['src/metrics-exporter.c',
                      'src/varlink-io.systemd.Metrics.c',
                      'src/metrics-varlink.c', 'src/selinux-metrics.c',
                      'src/network-metrics.c']

executable('metrics-exporter',
           metrics_exporter_c,
           include_directories : inc,
	   link_with : [libcommon],
           dependencies : [libeconf, libsystemd, libselinux, libnm, librpm],
           install : true,
	   install_dir: libexecdir)

# Generate config.h
#subdir('include')
subdir('etc')
subdir('systemd')
subdir('man')

# Unit tests
subdir('tests')

conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('SYSCONFDIR', sysconfdir)
conf.set_quoted('DATADIR', datadir)
conf.set_quoted('CONFIGDIR', configdir)

config_h = configure_file(
  output : 'config.h',
  configuration : conf)
