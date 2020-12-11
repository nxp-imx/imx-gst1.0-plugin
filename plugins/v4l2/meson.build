imxv4l2_sources = ['gstimxv4l2plugins.c', 'gstimxv4l2allocator.c', 'gstimxv4l2sink.c', 'gstimxv4l2src.c']
imxv4l2_headers = ['gstimxv4l2allocator.h', 'gstimxv4l2sink.h', 'gstimxv4l2src.h']

libs_device_g2d_inc = include_directories('../../libs/device-2d')
libs_v4l2_core_inc = include_directories('../../libs/v4l2_core')
libs_overlaycompositionmeta_inc = include_directories('../../libs/overlaycompositionmeta')
libs_video_overlay_inc = include_directories('../../libs/video-overlay')

allocator_dep= unneeded_dep
if have_bad_allocator
  allocator_dep = gst_bad_allocator_dep
else
  allocator_dep = gst_allocator_dep
endif

gstimxv4l2 = library('gstimxv4l2',
  imxv4l2_sources + imxv4l2_headers,
  c_args: version_flags + ionallocator_flags + dmabufheapsallocator_flags,
  link_args : gst_plugin_ldflags,
  include_directories : [libsinc, libs_device_g2d_inc, libs_v4l2_core_inc, libs_overlaycompositionmeta_inc, libs_video_overlay_inc],
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gst_video_dep, allocator_dep, gstfsl_dep],
  install : true,
  install_dir : plugins_install_dir,
)
