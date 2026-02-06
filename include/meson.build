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
  build_always_stale: true
)

global_include_dep = declare_dependency(
  sources: [setup_h, serno_h],
  include_directories: include_directories('.'),
)
