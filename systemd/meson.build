systemd = dependency('systemd', required: false)
if systemd.found()
  unitdir = systemd.get_variable(pkgconfig: 'systemd_system_unit_dir')
  tmpfilesdir = systemd.get_variable(pkgconfig: 'tmpfiles_dir')
else
  unitdir = get_option('prefix') / 'lib/systemd/system'
  tmpfilesdir = get_option('prefix') / 'lib/tmpfiles.d'
endif

install_data(
  'metrics-exporter.socket',
  install_dir: unitdir,
)
install_data(
  'metrics-exporter@.service',
  install_dir: unitdir,
)
