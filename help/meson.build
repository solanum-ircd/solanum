# Generate help index files
help_index = custom_target('help_index',
  output: ['help_index'],
  command: [
    'sh', '@SOURCE_ROOT@/help/make_index.SH', '@SOURCE_ROOT@', '@BUILD_ROOT@/@OUTPUT0@'
  ],
  build_by_default: true,
  build_always_stale: true
)

# Symlinks from users -> opers for shared help topics
user_symlinks = [
  'topic', 'accept', 'cmode', 'admin', 'names', 'links', 'away', 'whowas',
  'version', 'kick', 'who', 'invite', 'quit', 'join', 'list', 'nick',
  'oper', 'part', 'time', 'credits', 'motd', 'userhost', 'users', 'whois',
  'ison', 'lusers', 'user', 'help', 'pass', 'error', 'challenge', 'knock',
  'ping', 'pong', 'map', 'trace', 'chantrace', 'extban', 'monitor', 'batch'
]

install_subdir('users',
  install_dir: helpdir,
  exclude_files: ['index.tmp'] + user_symlinks
)

install_subdir('opers',
  install_dir: helpdir,
  exclude_files: ['index.tmp']
)

foreach link : user_symlinks
  install_symlink(link,
    pointing_to: '../opers/' + link,
    install_dir: helpdir / 'users'
  )
endforeach
