cargs = ['-DG_LOG_DOMAIN="PowerSBU"']

sbu_gui_resources = gnome.compile_resources(
  'sbu-gui-resources',
  'sbu-gui.gresource.xml',
  source_dir : '.',
  c_name : 'sbu'
)

sbu_dbus_src = gnome.gdbus_codegen(
  'generated-gdbus',
  'com.hughski.PowerSBU.xml',
  interface_prefix : 'com.hughski.PowerSBU.',
  namespace : 'Sbu')

executable(
  'sbu-gui',
  sbu_gui_resources,
  sources : [
    'egg-graph-point.c',
    'egg-graph-widget.c',
    'sbu-common.c',
    'sbu-config.c',
    'sbu-database.c',
    'sbu-gui.c',
    'sbu-xml-modifier.c',
    sbu_dbus_src
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    gtk,
    sqlite3,
    appstream_glib,
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
    sbu_dbus_src
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
    'sbu-device-impl.c',
    'sbu-link-impl.c',
    'sbu-node-impl.c',
    'sbu-manager-impl.c',
    'sbu-plugin.c',
    'sbu-main.c',
    sbu_dbus_src
  ],
  include_directories : [
    include_directories('..'),
  ],
  dependencies : [
    gio,
    gusb,
    sqlite3,
    gmodule,
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
      'sbu-self-test.c',
      'sbu-xml-modifier.c',
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
