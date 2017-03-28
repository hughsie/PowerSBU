i18n.merge_file(
  input: 'com.hughski.MsxGui.desktop.in',
  output: 'com.hughski.MsxGui.desktop',
  type: 'desktop',
  po_dir: join_paths(meson.source_root(), 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

i18n.merge_file(
  input: 'com.hughski.MsxGui.appdata.xml.in',
  output: 'com.hughski.MsxGui.appdata.xml',
  type: 'xml',
  po_dir: join_paths(meson.source_root(), 'po'),
  install: true,
  install_dir: join_paths(get_option('datadir'), 'appdata')
)
