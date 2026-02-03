project('solanum', 'c',
  version: '1.0-dev',
  default_options: ['c_std=c99', 'warning_level=0'],
  meson_version: '>=0.61.0', # for install_symlink()
  license: 'GPLv2'
)
cc = meson.get_compiler('c')

# Required dependencies
flex = find_program('flex', 'lex', required: true)
bison = find_program('bison', 'yacc', 'byacc', required: true)
socket_dep = cc.find_library('socket', required: false)
sqlite3_dep = dependency('sqlite3', required: true)
dl_dep = cc.find_library('dl', required: false)
ltdl_dep = cc.find_library('ltdl', required: true)
crypt_dep = cc.find_library('crypt', required: false)
if not crypt_dep.found()
  crypt_dep = cc.find_library('descrypt', required: false)
endif

# Hyperscan
hyperscan_dep = dependency('', required: false)
have_hyperscan = false
if get_option('hyperscan').allowed()
  hyperscan_dep = dependency('libhs', version: '>=4', required: get_option('hyperscan'))
  if hyperscan_dep.found()
    have_hyperscan = true
  endif
endif

# SCTP
sctp_dep = dependency('', required: false)
have_sctp = false
if get_option('sctp').allowed()
  sctp_lib = cc.find_library('sctp', required: get_option('sctp'))
  if sctp_lib.found() and cc.has_header('netinet/sctp.h')
    if cc.has_function('sctp_bindx', dependencies: sctp_lib)
      sctp_dep = sctp_lib
      have_sctp = true
    endif
  endif
endif

# Priority: mbedTLS > OpenSSL > GnuTLS
tls_backend = 'none'
tls_deps = []
have_openssl = false
have_mbedtls = false
have_gnutls = false

# mbedTLS
if get_option('mbedtls').allowed()
  mbedtls_lib = cc.find_library('mbedtls', required: false)
  mbedx509_lib = cc.find_library('mbedx509', required: false)
  mbedcrypto_lib = cc.find_library('mbedcrypto', required: false)
  if mbedtls_lib.found() and mbedx509_lib.found() and mbedcrypto_lib.found()
    tls_backend = 'mbedtls'
    tls_deps = [mbedtls_lib, mbedx509_lib, mbedcrypto_lib]
    have_mbedtls = true
  endif
endif

# OpenSSL
if tls_backend == 'none' and get_option('openssl').allowed()
  openssl_dep = dependency('openssl', version: '>=0.9.7', required: false)
  if openssl_dep.found()
    tls_backend = 'openssl'
    tls_deps = [openssl_dep]
    have_openssl = true
  endif
endif

# GnuTLS
if tls_backend == 'none' and get_option('gnutls').allowed()
  gnutls_dep = dependency('gnutls', required: false)
  if gnutls_dep.found()
    tls_backend = 'gnutls'
    tls_deps = [gnutls_dep]
    have_gnutls = true
  endif
endif

if get_option('openssl').enabled() and not have_openssl
  error('OpenSSL requested but not found')
endif
if get_option('mbedtls').enabled() and not have_mbedtls
  error('mbedTLS requested but not found')
endif
if get_option('gnutls').enabled() and not have_gnutls
  error('GnuTLS requested but not found')
endif

message('TLS backend: ' + tls_backend)

fhs_paths = get_option('fhs_paths')
prefix = get_option('prefix')

# Convert standard Meson directories to absolute paths
libdir = prefix / get_option('libdir')
bindir = prefix / get_option('bindir')
libexecdir = prefix / get_option('libexecdir')
datadir = prefix / get_option('datadir')
localstatedir = prefix / get_option('localstatedir')
sysconfdir_base = prefix / get_option('sysconfdir')

if fhs_paths
  pkglibexecdir = libexecdir / meson.project_name()
  pkgrundir = prefix / 'run' / meson.project_name()
  pkglocalstatedir = localstatedir / 'lib' / meson.project_name()

  default_logdir = localstatedir / 'log' / meson.project_name()
  default_helpdir = datadir / meson.project_name() / 'help'
  default_moduledir = libdir / meson.project_name() / 'modules'
else
  pkglibexecdir = bindir
  pkgrundir = sysconfdir_base
  pkglocalstatedir = sysconfdir_base

  default_logdir = prefix / 'logs'
  default_helpdir = prefix / 'help'
  default_moduledir = prefix / 'modules'
endif

# User-overridable options with dynamic defaults
option_defaults = {
  'logdir': default_logdir,
  'helpdir': default_helpdir,
  'moduledir': default_moduledir,
  'confdir': sysconfdir_base,
  'custom_branding': meson.project_name(),
  'custom_version': meson.project_version(),
}
foreach varname, default : option_defaults
  opt = get_option(varname)
  set_variable(varname, opt == '' ? default : opt)
endforeach

# rundir default depends on fhs_paths and confdir
default_rundir = fhs_paths ? prefix / 'run' : confdir
rundir_opt = get_option('rundir')
rundir = rundir_opt == '' ? default_rundir : rundir_opt


conf_data = configuration_data()

# Basic defines
conf_data.set_quoted('PACKAGE_NAME', meson.project_name())
conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())

# Path defines
conf_data.set_quoted('IRCD_PREFIX', prefix)
conf_data.set_quoted('ETC_DIR', confdir)
conf_data.set_quoted('LOG_DIR', logdir)
conf_data.set_quoted('HELP_DIR', helpdir)
conf_data.set_quoted('MODULE_DIR', moduledir)
conf_data.set_quoted('PKGLIBEXECDIR', pkglibexecdir)
conf_data.set_quoted('PKGLOCALSTATEDIR', pkglocalstatedir)
conf_data.set_quoted('PKGRUNDIR', rundir)

# FHS paths
if fhs_paths
  conf_data.set('ENABLE_FHS_PATHS', 1)
endif

# Protocol limits
conf_data.set('NICKLEN', get_option('nicklen') + 1)
conf_data.set('TOPICLEN', get_option('topiclen'))

# Branding
conf_data.set_quoted('BRANDING_NAME', custom_branding)
conf_data.set_quoted('BRANDING_VERSION', custom_version)

if get_option('custom_branding') != ''
  conf_data.set('CUSTOM_BRANDING', 1)
endif

# Program prefix
program_prefix = get_option('program_prefix')
conf_data.set_quoted('PROGRAM_PREFIX', program_prefix)

# Features
if get_option('oper_chghost')
  conf_data.set('ENABLE_OPER_CHGHOST', 1)
endif
if have_sctp
  conf_data.set('HAVE_LIBSCTP', 1)
endif
if have_hyperscan
  conf_data.set('HAVE_HYPERSCAN', 1)
endif

# TLS backend defines
if have_openssl
  conf_data.set('HAVE_OPENSSL', 1)
endif
if have_mbedtls
  conf_data.set('HAVE_MBEDTLS', 1)
endif
if have_gnutls
  conf_data.set('HAVE_GNUTLS', 1)
endif

# Performance
assert_level = get_option('assert')
if assert_level == 'none'
  conf_data.set('NDEBUG', 1)
elif assert_level == 'soft'
  conf_data.set('SOFT_ASSERT', 1)
  conf_data.set('NDEBUG', 1)
endif
if get_option('profile')
  conf_data.set('SOLANUM_PROFILE', 1)
endif

# Heap sizes
conf_data.set('NICKNAMEHISTORYLENGTH', 15000) # Size of the WHOWAS array.
conf_data.set('CHANNEL_HEAP_SIZE', 8192)
conf_data.set('BAN_HEAP_SIZE', 4096)
conf_data.set('CLIENT_HEAP_SIZE', 8192)
conf_data.set('LCLIENT_HEAP_SIZE', 1024)
conf_data.set('PCLIENT_HEAP_SIZE', 256)
conf_data.set('USER_HEAP_SIZE', 8192)
conf_data.set('DNODE_HEAP_SIZE', 8192)
conf_data.set('TOPIC_HEAP_SIZE', 4096)
conf_data.set('LINEBUF_HEAP_SIZE', 2048)
conf_data.set('MEMBER_HEAP_SIZE', 32768)
conf_data.set('ND_HEAP_SIZE', 512)
conf_data.set('CONFITEM_HEAP_SIZE', 256)
conf_data.set('MONITOR_HEAP_SIZE', 1024)
conf_data.set('FD_HEAP_SIZE', 1024)
conf_data.set('AWAY_HEAP_SIZE', 512)

# System checks
system_checks = {
  'HAVE_CRYPT_H': cc.has_header('crypt.h'),
  'HAVE_SYS_PARAM_H': cc.has_header('sys/param.h'),
  'HAVE_SYS_SYSLOG_H': cc.has_header('sys/syslog.h'),
  'HAVE_SYS_EPOLL_H': cc.has_header('sys/epoll.h'),
  'HAVE_MACHINE_ENDIAN_H': cc.has_header('machine/endian.h'),
  'HAVE_STRLCAT': cc.has_function('strlcat'),
  'HAVE_STRLCPY': cc.has_function('strlcpy'),
}
foreach define, found : system_checks
  if found
    conf_data.set(define, 1)
  endif
endforeach

if not cc.has_header('stdarg.h')
  error('stdarg.h is required')
endif

conf_data.set_quoted('PATH_DEVNULL', '/dev/null')

if cc.has_type('uintptr_t', prefix: '#include <stdint.h>')
  conf_data.set('HAVE_UINTPTR_T', 1)
endif

# For compat with autotools. See https://www.gnu.org/software/libtool/manual/html_node/Autoconf-macros.html#index-LT_005fSYS_005fMODULE_005fEXT
if host_machine.system() == 'windows'
  conf_data.set_quoted('LT_MODULE_EXT', '.dll')
elif host_machine.system() == 'darwin'
  conf_data.set_quoted('LT_MODULE_EXT', '.so')
else
  conf_data.set_quoted('LT_MODULE_EXT', '.so')
endif

setup_h = configure_file(
  output: 'setup.h',
  configuration: conf_data,
  install: false
)

serno_h = custom_target('serno.h',
  output: 'serno.h',
  capture: true,
  command: [
    'sh', '-c',
    '''
    if [ -d .git ]; then
      revh=$(git rev-list -1 --no-commit-header --date=format:%Y%m%d --format=%cd-%h HEAD 2>/dev/null || true)
      datecode=$(git rev-list -1 --no-commit-header --format=%ct HEAD 2>/dev/null || true)
      if [ -n "$revh" ] && [ -n "$datecode" ]; then
        echo "#define SERNO \\"$revh\\""
        echo "#define DATECODE ${datecode}UL"
        exit 0
      fi
    fi
    echo '#define SERNO "unknown"'
    echo '#define DATECODE 0UL'
    '''
  ],
  install: false,
  build_by_default: true,
  build_always_stale: true
)

c_args = []
link_args = []

if get_option('warnings')
  c_args += [
    '-Wall',
    '-Wpointer-arith',
    '-Wimplicit',
    '-Wnested-externs',
    '-Wcast-align',
    '-Wcast-qual',
    '-Wwrite-strings',
    '-Werror=implicit-function-declaration',
    '-Wstrict-prototypes',
    '-Wmissing-prototypes',
    '-Wmissing-declarations',
    '-Wparentheses',
    '-Wextra',
    '-Wshadow',
    '-Wmissing-noreturn',
    '-Wundef',
    '-Wpacked',
    '-Wredundant-decls',
    '-Wfloat-equal',
    '-Wformat=2',
  ]
  c_args = cc.get_supported_arguments(c_args)
endif

if get_option('profile')
  if cc.has_argument('-pg')
    c_args += ['-pg']
    link_args += ['-pg']
  endif
endif

if get_option('asan')
  if cc.has_argument('-fsanitize=address')
    c_args += ['-fsanitize=address']
    link_args += ['-fsanitize=address']
  endif
endif

rpath_arg = '-Wl,-rpath,' + prefix / get_option('libdir')
link_args += [rpath_arg]

add_project_arguments(c_args, language: 'c')
add_project_link_arguments(link_args, language: 'c')
add_project_arguments('-D_GNU_SOURCE', language: 'c')
add_project_arguments('-I' + meson.current_build_dir(), language: 'c')

subdir('librb')
subdir('ircd')
subdir('ssld')
subdir('authd')
subdir('bandb')
subdir('tools')
subdir('modules')
subdir('extensions')
subdir('help')
subdir('doc')

subdir('tests')

install_emptydir(logdir)
install_emptydir(pkglocalstatedir) # needed for FHS paths

summary({
  'prefix': prefix,
  'confdir': confdir,
  'logdir': logdir,
  'helpdir': helpdir,
  'moduledir': moduledir,
  'pkglocalstatedir': pkglocalstatedir,
  'FHS paths': fhs_paths,
}, section: 'Directories')

summary({
  'TLS backend': tls_backend,
  'SCTP': have_sctp,
  'Hyperscan': have_hyperscan,
  'OPER CHGHOST': get_option('oper_chghost'),
}, section: 'Features')

summary({
  'Assertions': assert_level,
  'Profile': get_option('profile'),
  'Warnings': get_option('warnings'),
  'AddressSanitizer': get_option('asan'),
}, section: 'Debug Options')

summary({
  'Nickname length': get_option('nicklen'),
  'Topic length': get_option('topiclen'),
}, section: 'Protocol Limits')

summary({
  'Branding name': custom_branding,
  'Branding version': custom_version,
  'Program prefix': program_prefix,
}, section: 'Branding')
