install_data(
  'rebootmgr.conf',
  install_dir: configdir,
)

polkit_gobject_dep = dependency('polkit-gobject-1', required : false)
if polkit_gobject_dep.found()
  polkitdir = polkit_gobject_dep.get_variable(pkgconfig : 'policydir')
else
  polkitdir = join_paths(datadir, 'polkit-1/actions')
endif

install_data(
  'org.openSUSE.rebootmgr.policy',
  install_dir: polkitdir,
)

