i18n.merge_file(
  input: 'com.hughski.PowerSBU.desktop.in',
  output: 'com.hughski.PowerSBU.desktop',
  type: 'desktop',
  po_dir: join_paths(meson.source_root(), 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

i18n.merge_file(
  input: 'com.hughski.PowerSBU.appdata.xml.in',
  output: 'com.hughski.PowerSBU.appdata.xml',
  type: 'xml',
  po_dir: join_paths(meson.source_root(), 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'appdata')
)

install_data(['sbud.conf'],
  install_dir : join_paths(get_option('sysconfdir'), 'sbud')
)

install_data(['com.hughski.PowerSBU.conf'],
  install_dir : join_paths(get_option('sysconfdir'), 'dbus-1', 'system.d')
)

con2 = configuration_data()
con2.set('LIBEXECDIR',
         join_paths(get_option('prefix'),
                    get_option('libexecdir')))

# replace @libexecdir@
configure_file(
  input : 'sbud.service.in',
  output : 'sbud.service',
  configuration : con2,
  install: true,
  install_dir: join_paths(get_option('prefix'),
                          'lib',
                          'systemd',
                          'system'),
)

docbook2man = find_program('docbook2man', required : false)
if docbook2man.found()
  custom_target('sbu-util-man',
    output : 'sbu-util.1',
    input : 'sbu-util.sgml',
    command : [docbook2man, '@INPUT@', '--output', 'data'],
    install : true,
    install_dir : join_paths(get_option('datadir'), 'man', 'man1')
  )
  custom_target('sbu-gui-man',
    output : 'sbu-gui.1',
    input : 'sbu-gui.sgml',
    command : [docbook2man, '@INPUT@', '--output', 'data'],
    install : true,
    install_dir : join_paths(get_option('datadir'), 'man', 'man1')
  )
endif
