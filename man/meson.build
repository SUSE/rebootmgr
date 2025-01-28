
feature_man = get_option('man')
enable_man = not feature_man.disabled()
if enable_man
  prog_xsltproc = find_program(
    'xsltproc',
    required: feature_man,
    disabler: true,
  )
  prog_xmlcatalog = find_program(
    'xmlcatalog',
    required: feature_man,
    disabler: true,
  )
  xml_catalog = get_option('xml-catalog')

  man_stylesheet = get_option('man-stylesheet')
  have = run_command(
    [prog_xmlcatalog, '--noout', xml_catalog, man_stylesheet],
    check: feature_man.enabled()
  ).returncode() == 0
  if not have
    man_stylesheet = disabler()
  endif

  foreach man: [['rebootmgr.conf.5'], ['rebootmgrctl.1']]
    xml = man[0] + '.xml'

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

  # rebootmgrd.8.xml is special, as it creates 3 files
  custom_target('rebootmgrd.8',
    input: ['rebootmgrd.8.xml'],
    output: ['rebootmgrd.8', 'rebootmgr.service.8'],
    command: [prog_xsltproc,
              '-o', '@OUTPUT0@',
              '--path', meson.current_source_dir(),
              '--xinclude',
              '--nonet',
              man_stylesheet,
              '@INPUT@'],
    install: true,
    install_dir: mandir / 'man8',
    install_tag: 'man',
  )
endif
