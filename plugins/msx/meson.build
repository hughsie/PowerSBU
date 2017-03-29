cargs = ['-DG_LOG_DOMAIN="SbuPluginMsx"']

shared_module('sbu_plugin_msx',
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
  dependencies : [gusb, libm]
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
