# Documentation tools run on the build machine, including during cross builds.
makeinfo = find_program(
  'makeinfo',
  required: get_option('documentation'),
)
if makeinfo.found()
  doc_version = configuration_data()
  doc_version.set('VERSION', meson.project_version())
  version_texi = configure_file(
    input: 'version.texi.in',
    output: 'version.texi',
    configuration: doc_version,
  )
  doc_driver = files('../tools/build/build_doc.py')
  html = custom_target(
    'quickjs-html',
    input: 'quickjs.texi',
    output: 'quickjs.html',
    depend_files: version_texi,
    command: [
      python,
      doc_driver,
      'html',
      makeinfo,
      '@INPUT@',
      '@OUTPUT@',
      meson.current_build_dir(),
    ],
    build_by_default: false,
  )
  alias_target('docs', html)
  texi2pdf = find_program(
    'texi2pdf',
    required: false,
  )
  if texi2pdf.found()
    pdf = custom_target(
      'quickjs-pdf',
      input: 'quickjs.texi',
      output: 'quickjs.pdf',
      depend_files: version_texi,
      command: [
        python,
        doc_driver,
        'pdf',
        texi2pdf,
        '@INPUT@',
        '@OUTPUT@',
        meson.current_build_dir(),
      ],
      build_by_default: false,
    )
    alias_target('docs-pdf', pdf)
  endif
endif
