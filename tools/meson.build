tools_inc = include_directories('../include', '../librb/include')

tools = {
  'mkpasswd': [librb_dep, crypt_dep],
  'mkfingerprint': [librb_dep],
}
foreach name, deps : tools
  executable('solanum-' + name,
    name + '.c',
    dependencies: deps,
    include_directories: tools_inc,
    install: true,
    install_dir: get_option('bindir')
  )
endforeach
