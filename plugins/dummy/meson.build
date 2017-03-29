cargs = ['-DG_LOG_DOMAIN="SbuPluginDummy"']

shared_module('sbu_plugin_dummy',
  sources : [
    'sbu-plugin-dummy.c',
  ],
  include_directories : [
    include_directories('../..'),
    include_directories('../../src'),
  ],
  install : true,
  install_dir: plugin_dir,
  c_args : cargs,
  dependencies : [libm, gio]
)
