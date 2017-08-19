cargs = ['-DG_LOG_DOMAIN="SbuPluginMsx"']

shared_module('sbu_plugin_msx',
  sbu_dbus_src,
  sources : [
    'msx-common.c',
    'msx-context.c',
    'msx-device.c',
    'sbu-plugin-msx.c',
  ],
  include_directories : [
    include_directories('../..'),
    include_directories('../../src'),
  ],
  install : true,
  install_dir: plugin_dir,
  c_args : cargs,
  dependencies : [gusb, libm, gio]
)

executable(
  'msx-util',
  sources : [
    'msx-common.c',
    'msx-context.c',
    'msx-device.c',
    'msx-util.c',
  ],
  include_directories : [
    include_directories('../..'),
  ],
  dependencies : [
    gio,
    gusb,
    libm,
  ],
  c_args : cargs,
)

if get_option('enable-tests')
  e = executable(
    'msx-self-test',
    sources : [
      'msx-common.c',
      'msx-self-test.c'
    ],
    include_directories : [
      include_directories('../..'),
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
