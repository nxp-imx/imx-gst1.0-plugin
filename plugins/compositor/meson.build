imxcompositor_sources = ['gstimxcompositor.c', 'gstimxcompositorpad.c']
imxcompositor_headers = ['gstimxcompositor.h', 'gstimxcompositorpad.h']

libs_device_g2d_inc = include_directories('../../libs/device-2d')
libs_overlaycompositionmeta_inc = include_directories('../../libs/overlaycompositionmeta')

gstbad_dep = unneeded_dep
if have_bad_video
  gstbad_dep = gst_bad_base_dep
endif

if have_bad_base
  gstbad_dep = gst_bad_video_dep
endif

allocator_dep= unneeded_dep
if have_bad_allocator
  allocator_dep = gst_bad_allocator_dep
else
  allocator_dep = gst_allocator_dep
endif

gstimxcompositor = library('gstimxcompositor',
  imxcompositor_sources + imxcompositor_headers,
  c_args: version_flags + ionallocator_flags + dmabufheapsallocator_flags,
  link_args : gst_plugin_ldflags,
  include_directories : [libsinc, libs_device_g2d_inc, libs_overlaycompositionmeta_inc],
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gst_video_dep, gstbad_dep, allocator_dep, gstfsl_dep],
  install : true,
  install_dir : plugins_install_dir,
)
