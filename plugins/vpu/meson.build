gstvpu_sources = [
  'gstvpu.c',
  'gstvpuplugins.c',
  'gstvpudec.c',
  'gstvpudecobject.c',
  'gstvpuallocator.c',
  'gstvpuenc.c',
]

gstvpu_headers = [
  'gstvpu.h',
  'gstvpudec.h',
  'gstvpudecobject.h',
  'gstvpuallocator.h',
  'gstvpuenc.h',
]

allocator_dep= unneeded_dep
if have_bad_allocator
  allocator_dep = gst_bad_allocator_dep
else
  allocator_dep = gst_allocator_dep
endif

hantro_flags = []
if cc.has_header('hantro_enc/ewl.h')
  hantro_flags += ['-DUSE_H1_ENC']
endif

if cc.has_header('hantro_VC8000E_enc/ewl.h')
  hantro_flags += ['-DUSE_VC8000E_ENC']
endif

gstvpu = library('gstvpu',
  gstvpu_sources + gstvpu_headers,
  c_args: version_flags + ionallocator_flags + dmabufheapsallocator_flags + hantro_flags,
  link_args : gst_plugin_ldflags,
  include_directories : [extinc, libsinc],
  dependencies : [gst_dep, gst_base_dep, gst_plugins_base_dep, gst_video_dep, allocator_dep, vpuwrap_dep, gstfsl_dep],
  install : true,
  install_dir : plugins_install_dir,
)
