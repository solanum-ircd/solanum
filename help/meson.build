# Generate help index files
meson.add_install_script(
  'sh',
  '-c',
  'helpdir=${DESTDIR}/$0 && echo Installing help indexes to ${helpdir} && ${MESON_SOURCE_ROOT}/help/make_index.SH ${MESON_SOURCE_ROOT}/help ${helpdir}',
  helpdir,
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
