beepdec_cflags = []
if platform_is_aarch64 == 'no'
  beepdec_cflags += ['-D_ARM12']
endif

gst_audio_dep = cc.find_library('gstaudio-' + api_version)

beepdec_sources = [ 'beep.c', 'beepdec.c', 'beepregistry.c', 'beeptypefind.c']
beepdec_headers = ['beepdec.h', 'beepregistry.h']
gstbeepdec = library('gstbeepdec',
  beepdec_sources + beepdec_headers,
  c_args: version_flags + beepdec_cflags,
  link_args : gst_plugin_ldflags + ['-ldl'],
  include_directories : [extinc, libsinc],
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gsttag_dep, gstriff_dep, gstfsl_dep, gst_audio_dep],
  install : true,
  install_dir : plugins_install_dir,
)

acodec_libdir = fslaudiocodec_dep.get_pkgconfig_variable('libdir')

beep_registry = configuration_data()
beep_registry_install_dir = '@0@/share'.format(get_option('prefix'))

check_acodecs = [
  ['aacplusd', 'arm9'],
  ['aacplusd', 'arm11'],
  ['aacplusd', 'arm12'],
  ['aacplusd', 'arm'],
  ['aacd', 'arm9'],
  ['aacd', 'arm11'],
  ['aacd', 'arm12'],
  ['aacd', 'arm'],
  ['mp3d', 'arm9'],
  ['mp3d', 'arm11'],
  ['mp3d', 'arm12'],
  ['mp3d', 'arm'],
  ['wma10d', 'arm9'],
  ['wma10d', 'arm11'],
  ['wma10d', 'arm12'],
  ['wma10d', 'arm'],
  ['ac3d', 'arm11'],
  ['ac3d', 'arm12'],
  ['ac3d', 'arm'],
  ['vorbisd', 'arm11'],
  ['vorbisd', 'arm12'],
  ['vorbisd', 'arm'],
  ['ddpd', 'arm12'],
  ['ddpd', 'arm'],
  ['nbamrd', 'arm11'],
  ['nbamrd', 'arm'],
  ['wbamrd', 'arm12'],
  ['wbamrd', 'arm'],
  ['realad', 'arm9'],
  ['realad', 'arm11'],
  ['realad', 'arm'],
  ['dsp', 'arm'],
]
foreach acodec : check_acodecs
  filename = '@0@/@1@/wrap/lib_@2@_wrap_@3@_elinux.so'.format(sys_root, acodec_libdir, acodec.get(0),acodec.get(1))
  soname = run_command(python3, '../find_soname.py', filename).stdout().strip()
  if soname == 'file_not_exist'
    soname = 'lib_@0@_wrap_@1@_elinux.so'.format(acodec.get(0),acodec.get(1))
  endif
  beep_registry.set('@0@_@1@_SONAME'.format(acodec.get(0),acodec.get(1)), '@0@/wrap/@1@'.format(acodec_libdir,soname))
endforeach

beep_registry_files = [
  'beep_registry_1.0',
]

if platform_is_aarch64 == 'yes'
  foreach p : beep_registry_files
    infile = p + '.arm.cf.in'
    outfile = p + '.arm.cf'
    configure_file(input : infile,
      output : outfile,
      configuration : beep_registry,
      install_dir : beep_registry_install_dir)
  endforeach
else
  foreach p : beep_registry_files
    infile = p + '.arm12.cf.in'
    outfile = p + '.arm12.cf'
    configure_file(input : infile,
      output : outfile,
      configuration : beep_registry,
      install_dir : beep_registry_install_dir)
  endforeach
endif