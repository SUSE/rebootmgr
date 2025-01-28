bashcompletiondir = get_option('bashcompletiondir')
if bashcompletiondir == ''
        bash_completion = dependency('bash-completion', required : false)
        if bash_completion.found()
                bashcompletiondir = bash_completion.get_variable(pkgconfig : 'completionsdir')
        else
                bashcompletiondir = datadir / 'bash-completion/completions'
        endif
endif

install_data('rebootmgrctl', install_dir : bashcompletiondir)
