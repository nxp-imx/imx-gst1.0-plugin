gstfsl_sources = [
  'video-tsm/mfw_gst_ts.c',
  'gstsutils/gstsutils.c',
  'device-2d/imx_2d_device.c',
  'device-2d/imx_2d_device_allocator.c',
  'overlaycompositionmeta/imxoverlaycompositionmeta.c',
  'video-overlay/gstimxvideooverlay.c',
  'gstimxcommon.c',
]

gstfsl_headers = [
  'video-tsm/mfw_gst_ts.h',
  'gstsutils/gstsutils.h',
  'device-2d/imx_2d_device.h',
  'device-2d/imx_2d_device_allocator.h',
  'overlaycompositionmeta/imxoverlaycompositionmeta.h',
  'video-overlay/gstimxvideooverlay.h',
]

v4l2core_sources = ['v4l2_core/gstimxv4l2.c']

v4l2core_headers = ['v4l2_core/gstimxv4l2.h']

if cc.has_header('linux/mxcfb.h') or cc.has_header('uapi/mxcfb.h')
  gstfsl_sources += ['v4l2_core/gstimxv4l2.c']
  gstfsl_headers += ['v4l2_core/gstimxv4l2.h']
endif

gstfsl_cflags = []
gstfsl_ldflags = []
if cc.has_header('g2d.h') and build_g2d
  gstfsl_sources += ['device-2d/imx_2d_device_g2d.c']
  gstfsl_cflags += ['-DUSE_G2D']
  gstfsl_ldflags += ['-lg2d']
endif

if cc.has_header('pxp_lib.h') and build_pxp
  gstfsl_sources += ['device-2d/imx_2d_device_pxp.c']
  gstfsl_cflags += ['-DUSE_PXP']
  gstfsl_ldflags += ['-lpxp']
  if build_MX8 == 'yes' or build_MX9 == 'yes'
    gstfsl_cflags += ['-DENABLE_EXTRA_PXP_FORMATS']
  endif
endif

if cc.has_header('linux/ipu.h') and build_ipu
  gstfsl_sources += ['device-2d/imx_2d_device_ipu.c']
  gstfsl_cflags += ['-DUSE_IPU']
endif

if cc.has_header('X11/Xlib.h')
  gstfsl_sources += ['video-overlay/gstimxxoverlay.c']
  gstfsl_headers += ['video-overlay/gstimxxoverlay.h']
  gstfsl_cflags += ['-DUSE_X11']
  gstfsl_ldflags += ['-lX11']
endif

bad_allocator_dep = unneeded_dep
if have_bad_allocator
  bad_allocator_dep = gst_bad_allocator_dep
endif

gstfsl = library('gstfsl-@0@'.format(api_version),
  gstfsl_sources + gstfsl_headers + v4l2core_sources + v4l2core_headers,
  c_args : ionallocator_flags + dmabufheapsallocator_flags + gstfsl_cflags,
  link_args : gstfsl_ldflags,
  version : '0.0.0',
  soversion : 0,
  install : true,
  install_dir : libs_install_dir,
  dependencies : [gst_dep, gst_base_dep, gst_video_dep, gst_allocator_dep, bad_allocator_dep],
)

gstfsl_dep = declare_dependency(link_with : gstfsl)

