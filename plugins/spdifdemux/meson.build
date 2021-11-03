imxspdifdemux_sources = ['gstspdifdemux.c']
imxmspdifdemux_headers = ['gstspdifdemux.h']
spdifparser_lib_dir = imx_parser_dep.get_pkgconfig_variable('libdir')
spdifparser_lib = 'spdifparser'
#['-L' + sys_root + spdifparser_lib_dir, '-l' + spdifparser_lib]
gst_audio_dep = cc.find_library('gstaudio-' + api_version)

gstspdifdemux= library('gstspdifdemux',
  imxspdifdemux_sources + imxmspdifdemux_headers,
  c_args: version_flags + ['-O2', '-fno-omit-frame-pointer', '-D_ARM12'],
  link_args : gst_plugin_ldflags + ['-ldl'],
  include_directories : [extinc, libsinc],
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gsttag_dep, gst_audio_dep, imx_parser_dep],
  install : true,
  install_dir : plugins_install_dir,
)

