# Create a copy of serno.h in librb's include directory for version.c
librb_include_dir = meson.current_build_dir() / 'include'

# replace with fs.copyfile if we bump the minimal Meson version to 1.64
librb_serno_h = custom_target('librb_serno.h',
  input: serno_h,
  output: 'serno_librb.h',
  command: ['sh', '-c', 'cp $0 $1', '@INPUT@', '@OUTPUT@'],
  depends: serno_h
)
