src_file = ['gplay2.c', 'playlist.c']

gstplay_dep = dependency('gstreamer-play-' + api_version)

executable('gplay-' + api_version,
  src_file,
  install: true,
  dependencies : [gst_dep, gstplay_dep, gstfsl_dep],
)
