src_file = ['gplay2.c', 'playlist.c']

gstplayer_dep = dependency('gstreamer-player-' + api_version)

executable('gplay-' + api_version,
  src_file,
  install: true,
  dependencies : [gst_dep, gstplayer_dep, gstfsl_dep],
)
