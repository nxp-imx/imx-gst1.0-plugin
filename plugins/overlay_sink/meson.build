overlaysink_sources = [ 'gstosink.c', 'gstosinkallocator.c', 'osink_object.c', 'compositor.c']
overlaysink_headers = [ 'osink_common.h', 'gstosink.h', 'gstosinkallocator.h', 'osink_object.h', 'compositor.h', 'displays.h']

libs_device_g2d_inc = include_directories('../../libs/device-2d')
libs_v4l2_core_inc = include_directories('../../libs/v4l2_core')
libs_overlaycompositionmeta_inc = include_directories('../../libs/overlaycompositionmeta')
libs_video_overlay_inc = include_directories('../../libs/video-overlay')
libs_gstsutils_inc = include_directories('../../libs/gstsutils')

allocator_dep= unneeded_dep
if have_bad_allocator
  allocator_dep = gst_bad_allocator_dep
else
  allocator_dep = gst_allocator_dep
endif

if build_MX7ULP == 'yes'
  overlaysink_sources += ['displaysfb.c']
else
  overlaysink_sources += ['displaysv4l2.c']
endif

gstoverlaysink = library('gstoverlaysink',
  overlaysink_sources + overlaysink_headers,
  c_args: version_flags + ionallocator_flags + dmabufheapsallocator_flags,
  link_args : gst_plugin_ldflags + ['-lg2d'],
  include_directories : [extinc, libsinc, libs_device_g2d_inc, libs_v4l2_core_inc, libs_overlaycompositionmeta_inc, libs_video_overlay_inc, libs_gstsutils_inc],
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gst_video_dep, allocator_dep, gstfsl_dep],
  install : true,
  install_dir : plugins_install_dir,
)

overlaysink_config_install_dir = '@0@/share'.format(get_option('prefix'))
config_files = ['imx_6q_display_config', 'imx_6sx_display_config', 'imx_7ulp_display_config', 'imx_8dv_display_config']
install_data(sources: config_files, install_dir: overlaysink_config_install_dir)
