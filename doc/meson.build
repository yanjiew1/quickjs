# Copyright (c) 2017-2021 Fabrice Bellard
# Copyright (c) 2017-2021 Charlie Gordon
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

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
