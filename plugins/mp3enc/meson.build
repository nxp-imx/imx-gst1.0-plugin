imxmp3enc_sources = [ 'gstimxmp3.c', 'gstimxmp3enc.c']
imxmp3enc_headers = ['gstimxmp3enc.h']
gstimxmp3enc= library('gstimxmp3enc',
  imxmp3enc_sources + imxmp3enc_headers,
  c_args: version_flags + ['-O2', '-fno-omit-frame-pointer', '-D_ARM12'],
  link_args : gst_plugin_ldflags + ['-l_mp3_enc_arm12_elinux'],
  include_directories : [extinc, libsinc],
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gsttag_dep, gstriff_dep, gstfsl_dep, gst_audio_dep],
  install : true,
  install_dir : plugins_install_dir,
)