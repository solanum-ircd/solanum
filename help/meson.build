# Generate help index files
help_index = custom_target('help_index',
  output: ['users_index', 'opers_index'],
  command: [
    'sh', '-c',
    '''
    OUTPUT0="$(cd @BUILD_ROOT@ && pwd)/@OUTPUT0@"
    OUTPUT1="$(cd @BUILD_ROOT@ && pwd)/@OUTPUT1@"
    cd @SOURCE_ROOT@/help

    # Build users index
    rm -f users/index.tmp
    for help in users/*; do
      if [ -f "$help" ]; then
        echo "$help" >> users/index.tmp
      fi
    done

    # Add symlinks to users index
    for link in topic accept cmode admin names links away whowas version kick who invite quit join list nick oper part time credits motd userhost users whois ison lusers user help pass error challenge knock ping pong map trace chantrace extban monitor batch; do
      echo "$link" >> users/index.tmp
    done

    echo 'Help topics available to users:' > users/index
    echo '' >> users/index
    cat users/index.tmp | sed -e 's|^users/||' | sort -u | tr a-z A-Z | column -c 65 -x | expand >> users/index
    rm -f users/index.tmp

    # Build opers index
    rm -f opers/index.tmp
    for help in opers/*; do
      if [ -f "$help" ]; then
        echo "$help" >> opers/index.tmp
      fi
    done

    echo 'Help topics available to opers:' > opers/index
    echo '' >> opers/index
    cat opers/index.tmp | sed -e 's|^opers/||' | sort -u | tr a-z A-Z | column -c 65 -s ' ' -x | expand >> opers/index
    rm -f opers/index.tmp

    touch $OUTPUT0 $OUTPUT1
    '''
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
