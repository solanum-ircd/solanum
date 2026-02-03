ircd_parser = custom_target('ircd_parser',
  input: 'ircd_parser.y',
  output: ['ircd_parser.c', 'ircd_parser.h'],
  command: [bison, '-d', '@INPUT@', '--output=@OUTPUT0@', '--defines=@OUTPUT1@']
)

ircd_lexer = custom_target('ircd_lexer',
  input: 'ircd_lexer.l',
  output: 'ircd_lexer.c',
  command: [flex, '-o', '@OUTPUT@', '@INPUT@']
)

credits_result = run_command('sed', 's/^$/ /;s/.*/  "&",/', meson.project_source_root() / 'CREDITS', check: true)
ircd_version_conf = configuration_data()
ircd_version_conf.set('GENERATION', '0')
ircd_version_conf.set('CREATION', '__DATE__ at __TIME__')
ircd_version_conf.set('CREDITS', credits_result.stdout())
ircd_version_conf.set('PACKAGE', meson.project_name())
ircd_version = configure_file(
  input: 'version.c.in',
  output: 'version.c',
  configuration: ircd_version_conf
)

libircd_sources = files(
  'authproc.c',
  'bandbi.c',
  'batch.c',
  'cache.c',
  'capability.c',
  'channel.c',
  'chmode.c',
  'class.c',
  'client.c',
  'client_tags.c',
  'dns.c',
  'extban.c',
  'getopt.c',
  'hash.c',
  'hook.c',
  'hostmask.c',
  'ircd.c',
  'ircd_signal.c',
  'listener.c',
  'logger.c',
  'match.c',
  'modules.c',
  'monitor.c',
  'msgbuf.c',
  'newconf.c',
  'operhash.c',
  'packet.c',
  'parse.c',
  'privilege.c',
  'ratelimit.c',
  'reject.c',
  'restart.c',
  's_conf.c',
  's_newconf.c',
  's_serv.c',
  's_user.c',
  'scache.c',
  'send.c',
  'snomask.c',
  'sslproc.c',
  'substitution.c',
  'supported.c',
  'tgchange.c',
  'whowas.c',
)

libircd_inc = include_directories('../include', '../librb/include')

libircd = shared_library('ircd',
  [libircd_sources, ircd_parser, ircd_lexer, ircd_version, serno_h],
  dependencies: [librb_dep, ltdl_dep, sqlite3_dep],
  include_directories: libircd_inc,
  install: true,
  install_dir: get_option('libdir')
)

libircd_dep = declare_dependency(
  link_with: libircd,
  include_directories: libircd_inc,
  dependencies: [librb_dep, ltdl_dep, sqlite3_dep]
)

solanum = executable('solanum',
  'main.c',
  dependencies: [libircd_dep],
  export_dynamic: true,
  install: true,
  install_dir: get_option('bindir')
)
