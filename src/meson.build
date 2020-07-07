viewer_installer_sources = [
  'utils.c',
  'main.c',
  'viewer-installer-application.c',
  'viewer-installer-window.c',
  'viewer-installer-window-view-model.c',
]

viewer_installer_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
  dependency('libgtop-2.0', version: '>=2.37.0'),
  dependency('glib-2.0',    version: '>=2.56.0'),
  dependency('json-glib-1.0', version : '>= 1.2.0'),
  dependency('libcurl')
]

gnome = import('gnome')

resources = gnome.compile_resources(
  'viewer-installer-resources',
  join_paths(
    'resources', 'viewer-installer.gresource.xml'
  ),
  source_dir: 'resources',
  c_name: 'viewer_installer',
)

viewer_installer_sources += resources[0]

executable('hancom-viewer-installer', viewer_installer_sources,
  dependencies: viewer_installer_deps,
  install: true,
)

install_data('viewer-installer-infos.json',
             install_dir : join_paths(get_option('libdir'), 'hancom-viewer-installer'))

install_data('hancom-viewer-check',
             install_dir : join_paths(get_option('libdir'), 'hancom-viewer-installer'))

install_data('hancom-viewer-install',
             install_dir : join_paths(get_option('libdir'), 'hancom-viewer-installer'))