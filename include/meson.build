setup_h = configure_file(
  output: 'setup.h',
  configuration: conf_data,
  install: false
)

serno_h = vcs_tag(
  command: ['git', 'rev-list', '-1', '--no-commit-header', '--date=format:%Y%m%d', '--format=%cd-%h', 'HEAD'],
  input: 'serno.h.in',
  output: 'serno.h',
  replace_string: '@SERNO@',
  fallback: 'unknown',
)

datecode_h = vcs_tag(
  command: ['git', 'rev-list', '-1', '--no-commit-header', '--format=%ct', 'HEAD'],
  input: 'datecode.h.in',
  output: 'datecode.h',
  replace_string: '@DATECODE@',
  fallback: '0',
)

global_include_dep = declare_dependency(
  sources: [setup_h, serno_h, datecode_h],
  include_directories: include_directories('.'),
)
