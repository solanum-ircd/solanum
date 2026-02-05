tap_sources = files(
  'tap/basic.c',
  'tap/float.c',
)

tap_lib = static_library('tap',
  tap_sources,
  include_directories: [include_directories('tap'), include_directories('..')]
)

test_util_sources = files(
  'ircd_util.c',
  'client_util.c',
)

test_utils = static_library('test_utils',
  [test_util_sources, serno_h],
  dependencies: [libircd_dep, librb_dep],
  include_directories: [include_directories('..')]
)

test_programs = {
  'chmode1': 'chmode1.c',
  'match1': 'match1.c',
  'misc': 'misc.c',
  'msgbuf_parse1': 'msgbuf_parse1.c',
  'msgbuf_unparse1': 'msgbuf_unparse1.c',
  'hostmask1': 'hostmask1.c',
  'privilege1': 'privilege1.c',
  'rb_dictionary1': 'rb_dictionary1.c',
  'rb_snprintf_append1': 'rb_snprintf_append1.c',
  'rb_snprintf_try_append1': 'rb_snprintf_try_append1.c',
  'sasl_abort1': 'sasl_abort1.c',
  'send1': 'send1.c',
  'send_multiline1': 'send_multiline1.c',
  'serv_connect1': 'serv_connect1.c',
  'substitution1': 'substitution1.c',
}

foreach test_name, test_source : test_programs
  test_exe = executable(test_name,
    test_source,
    dependencies: [libircd_dep, librb_dep, dl_dep],
    link_with: [test_utils, tap_lib],
    include_directories: [include_directories('..')],
    build_by_default: true
  )

  test(test_name, test_exe)
endforeach

runtests = executable('runtests',
  'runtests.c',
  c_args: [
    '-DC_TAP_SOURCE="' + meson.current_source_dir() + '"',
    '-DC_TAP_BUILD="' + meson.current_build_dir() + '"',
  ],
  link_with: [tap_lib],
  include_directories: [include_directories('tap'), include_directories('..')],
  build_by_default: true
)
