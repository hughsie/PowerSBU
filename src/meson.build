subdir('data')

cargs = ['-DG_LOG_DOMAIN="Msx"']

msx_gui_resources = gnome.compile_resources(
  'msx-gui-resources',
  'msx-gui.gresource.xml',
  source_dir : '.',
  c_name : 'msx'
)

executable(
  'msx-gui',
  msx_gui_resources,
  sources : [
    'egg-graph-point.c',
    'egg-graph-widget.c',
    'msx-database.c',
    'msx-gui.c',
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
  'msx-util',
  sources : [
    'msx-common.c',
    'msx-context.c',
    'msx-database.c',
    'msx-device.c',
    'msx-util.c',
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

if get_option('enable-tests')
  e = executable(
    'msx-self-test',
    sources : [
      'msx-common.c',
      'msx-database.c',
      'msx-self-test.c'
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
  test('msx-self-test', e)
endif
