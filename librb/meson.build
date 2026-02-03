fs = import('fs')
dl_dep = cc.find_library('dl', required: false)

librb_sources = files(
  'src/unix.c',
  'src/crypt.c',
  'src/balloc.c',
  'src/commio.c',
  'src/mbedtls.c',
  'src/openssl.c',
  'src/gnutls.c',
  'src/nossl.c',
  'src/event.c',
  'src/rb_lib.c',
  'src/rb_memory.c',
  'src/linebuf.c',
  'src/tools.c',
  'src/helper.c',
  'src/devpoll.c',
  'src/epoll.c',
  'src/poll.c',
  'src/ports.c',
  'src/sigio.c',
  'src/kqueue.c',
  'src/rawbuf.c',
  'src/patricia.c',
  'src/dictionary.c',
  'src/radixtree.c',
  'src/arc4random.c',
)

librb_credits_result = run_command('sed', 's/^$/ /;s/.*/  "&",/', meson.current_source_dir() / 'CREDITS', check: true)
librb_version_conf = configuration_data()
librb_version_conf.set('GENERATION', '0')
librb_version_conf.set('CREDITS', librb_credits_result.stdout())
librb_version_c = configure_file(
  input: 'src/version.c.in',
  output: 'version.c',
  configuration: librb_version_conf
)

# Create a copy of serno.h in librb's include directory for version.c
librb_include_dir = meson.current_build_dir() / 'include'
librb_serno_h = custom_target('librb_serno.h',
  input: serno_h,
  output: 'serno_librb.h',
  command: ['sh', '-c', 'mkdir -p ' + librb_include_dir + ' && cp @INPUT@ ' + librb_include_dir + '/serno.h && cp @INPUT@ @OUTPUT@'],
  depends: serno_h
)

librb_conf_data = configuration_data()

rt_dep = cc.find_library('rt', required: false)

librb_checks = {
  'HAVE_CRYPT_H': cc.has_header('crypt.h'),
  'HAVE_SYS_POLL_H': cc.has_header('sys/poll.h'),
  'HAVE_SYS_EPOLL_H': cc.has_header('sys/epoll.h'),
  'HAVE_SYS_SELECT_H': cc.has_header('sys/select.h'),
  'HAVE_SYS_DEVPOLL_H': cc.has_header('sys/devpoll.h'),
  'HAVE_SYS_EVENT_H': cc.has_header('sys/event.h'),
  'HAVE_PORT_H': cc.has_header('port.h'),
  'HAVE_SYS_SIGNALFD_H': cc.has_header('sys/signalfd.h'),
  'HAVE_SYS_TIMERFD_H': cc.has_header('sys/timerfd.h'),
  'TIME_WITH_SYS_TIME': cc.has_header('time.h'),
  'HAVE_GETEXECNAME': cc.has_function('getexecname'),
  'HAVE_STRLCPY': cc.has_function('strlcpy'),
  'HAVE_STRLCAT': cc.has_function('strlcat'),
  'HAVE_STRCASESTR': cc.has_function('strcasestr'),
  'HAVE_SIGNALFD': cc.has_function('signalfd'),
  'HAVE_ARC4RANDOM': cc.has_function('arc4random'),
  'HAVE_KEVENT': cc.has_function('kevent', prefix: '#include <sys/event.h>'),
  'HAVE_PORT_CREATE': cc.has_function('port_create', prefix: '#include <port.h>'),
  'HAVE_EPOLL_CTL': cc.has_function('epoll_ctl', prefix: '#include <sys/epoll.h>'),
  'HAVE_TIMERFD_CREATE': cc.has_function('timerfd_create', prefix: '#include <sys/timerfd.h>'),
  'HAVE_DLINFO': cc.has_function('dlinfo', dependencies: dl_dep),
  'HAVE_NANOSLEEP': cc.has_function('nanosleep', dependencies: rt_dep),
  'HAVE_TIMER_CREATE': cc.has_function('timer_create', dependencies: rt_dep, prefix: '#include <time.h>'),
  'HAVE_DEVPOLL': fs.exists('/dev/poll'),
  'SOCKADDR_IN_HAS_LEN': cc.has_member('struct sockaddr', 'sa_len', prefix: '#include <sys/types.h>\n#include <sys/socket.h>'),
}
foreach define, found : librb_checks
  if found
    librb_conf_data.set(define, 1)
  endif
endforeach

# TLS backend configuration (passed from parent)
if have_openssl
  librb_conf_data.set('HAVE_OPENSSL', 1)
endif

if have_mbedtls
  librb_conf_data.set('HAVE_MBEDTLS', 1)
endif

if have_gnutls
  librb_conf_data.set('HAVE_GNUTLS', 1)
endif

librb_dash_config_h = configure_file(
  input: 'librb-config.h.in',
  output: 'librb-config.h',
  configuration: librb_conf_data,
  install: false
)

# For compat with autotools mess
librb_underscore_config_h = configure_file(
  input: librb_dash_config_h,
  output: 'librb_config.h',
  copy: true
)

librb_deps = [socket_dep] + tls_deps
if rt_dep.found()
  librb_deps += [rt_dep]
endif
if sctp_dep.found()
  librb_deps += [sctp_dep]
endif

librb_inc = include_directories('include', '.', is_system: false)

librb_build_inc = include_directories('.', is_system: false)

librb = shared_library('rb',
  [librb_sources, librb_version_c, librb_dash_config_h, librb_underscore_config_h, librb_serno_h],
  dependencies: librb_deps,
  include_directories: [librb_inc, librb_build_inc],
  c_args: ['-I' + meson.current_build_dir()],
  install: true,
  install_dir: get_option('libdir')
)

librb_dep = declare_dependency(
  link_with: librb,
  include_directories: librb_inc,
  dependencies: librb_deps
)

pkg = import('pkgconfig')
pkg.generate(librb,
  name: 'librb',
  description: 'libratbox',
  subdirs: 'librb',
  extra_cflags: ['-I${includedir}/librb']
)
