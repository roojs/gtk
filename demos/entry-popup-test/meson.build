executable('gtk4-entry-popup-test',
  sources: 'main.c',
  dependencies: [ libgtk_dep ],
  include_directories: confinc,
  c_args: common_cflags,
  win_subsystem: 'windows',
  install: true,
  kwargs: demo_kwargs
)

configure_file(
  input: 'org.gtk.EntryPopupTest.appdata.xml.in',
  output: 'org.gtk.EntryPopupTest.appdata.xml',
  configuration: appdata_config,
  install_dir: gtk_appdatadir
)

install_data('org.gtk.EntryPopupTest.desktop', install_dir: gtk_applicationsdir)
