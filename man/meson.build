
foreach man: [['rebootmgr.conf.5'], ['rebootmgrctl.1'], ['rebootmgrd.8']]
  xml = man[0] + '.xml'

  run_command([prog_xmllint,
               '--nonet',
               '--noout',
               '--xinclude',
               '--relaxng', docbook_rng,
               xml],
              check: true)

  custom_target(man[0],
    input: xml,
    output: man,
    command: [prog_xsltproc,
              '-o', '@OUTPUT0@',
              '--path', meson.current_source_dir(),
              '--xinclude',
              '--nonet',
              man_stylesheet,
              '@INPUT@'],
    install: true,
    install_dir: mandir / 'man' + man[0].substring(-1),
    install_tag: 'man',
  )
endforeach

