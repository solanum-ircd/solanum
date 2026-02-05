setup_h = configure_file(
  output: 'setup.h',
  configuration: conf_data,
  install: false
)

serno_h = custom_target('serno.h',
  output: 'serno.h',
  capture: true,
  command: ['sh', '@CURRENT_SOURCE_DIR@/serno.h.SH'],
  install: false,
  build_by_default: true,
  build_always_stale: true
)

global_include_dep = declare_dependency(
  include_directories: include_directories('.'),
)
