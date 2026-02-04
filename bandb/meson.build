bandb_common_sources = files(
  'rsdb_sqlite3.c',
  'rsdb_snprintf.c',
)

bandb = executable('bandb',
  ['bandb.c'] + bandb_common_sources,
  dependencies: [librb_dep, sqlite3_dep],
  include_directories: include_directories('../include'),
  install: true,
  install_dir: pkglibexecdir
)

bantool = executable('solanum-bantool',
  ['bantool.c'] + bandb_common_sources,
  dependencies: [librb_dep, sqlite3_dep],
  include_directories: include_directories('../include'),
  install: true,
  install_dir: get_option('bindir')
)
