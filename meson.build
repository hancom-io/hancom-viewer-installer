project('hancom-viewer-installer', 'c',        version: '0.1.0',
  meson_version: '>= 0.40.0',
)

i18n = import('i18n')

config_h = configuration_data()
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name())
config_h.set_quoted('LOCALEDIR', join_paths(get_option('prefix'), get_option('localedir')))
config_h.set_quoted('DATADIR', join_paths(get_option('prefix'), get_option('datadir')))
config_h.set_quoted('LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
configure_file(
  output: 'viewer-installer-config.h',
  configuration: config_h,
)
add_project_arguments([
  '-I' + meson.build_root(),
], language: 'c')

subdir('data')
subdir('src')
subdir('po')

meson.add_install_script('build-aux/meson/postinstall.py')
