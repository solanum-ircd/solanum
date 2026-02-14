authd_sources = files(
  'authd.c',
  'dns.c',
  'notice.c',
  'provider.c',
  'res.c',
  'reslib.c',
  'providers/dnsbl.c',
  'providers/ident.c',
  'providers/opm.c',
  'providers/rdns.c',
)

authd = executable('authd',
  authd_sources,
  dependencies: [librb_dep],
  include_directories: include_directories('../include'),
  install: true,
  install_dir: pkglibexecdir,
  install_rpath: rpath,
)
