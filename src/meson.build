cargs = ['-DG_LOG_DOMAIN="PowerSBU"']

sbu_gui_resources = gnome.compile_resources(
  'sbu-gui-resources',
  'sbu-gui.gresource.xml',
  source_dir : '.',
  c_name : 'sbu'
)

executable(
  'sbu-gui',
  sbu_gui_resources,
  sources : [
    'egg-graph-point.c',
    'egg-graph-widget.c',
    'sbu-device.c',
    'sbu-node.c',
    'sbu-link.c',
    'sbu-common.c',
    'sbu-config.c',
    'sbu-database.c',
    'sbu-gui.c',
    'sbu-xml-modifier.c',
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    gtk,
    sqlite3,
    libm,
  ],
  c_args : cargs,
  install : true,
  install_dir : 'bin'
)

executable(
  'sbu-util',
  sources : [
    'sbu-common.c',
    'sbu-config.c',
    'sbu-database.c',
    'sbu-util.c',
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    gio,
    gusb,
    sqlite3,
    libm,
  ],
  c_args : cargs,
  install : true,
  install_dir : 'bin'
)

executable(
  'sbud',
  sources : [
    'sbu-common.c',
    'sbu-config.c',
    'sbu-database.c',
    'sbu-device.c',
    'sbu-dummy-plugin.c',
    'sbu-link.c',
    'sbu-main.c',
    'sbu-manager.c',
    'sbu-msx-common.c',
    'sbu-msx-device.c',
    'sbu-msx-plugin.c',
    'sbu-node.c',
    'sbu-plugin.c',
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    gio,
    gusb,
    sqlite3,
    libm,
    valgrind,
  ],
  c_args : cargs,
  install : true,
  install_dir : get_option('libexecdir')
)

if get_option('enable-tests')
  e = executable(
    'sbu-self-test',
    sources : [
      'sbu-common.c',
      'sbu-database.c',
      'sbu-msx-common.c',
      'sbu-self-test.c',
    ],
    include_directories : [
      include_directories('..'),
    ],
    dependencies : [
      gio,
      gusb,
      sqlite3,
      libm,
    ],
    c_args : cargs
  )
  test('sbu-self-test', e)
endif
