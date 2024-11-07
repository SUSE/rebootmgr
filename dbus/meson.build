foreach man: [['org.opensuse.RebootMgr.xml']]

  run_command([prog_xmllint,
               '--nonet',
               '--noout',
               '--valid',
               man],
              check: true)
endforeach

install_data(
  'org.opensuse.RebootMgr.conf',
  install_dir: busconfigdir,
)

install_data(
  'org.opensuse.RebootMgr.xml',
  install_dir: introspectiondir,
)
