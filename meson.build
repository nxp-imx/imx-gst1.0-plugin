project('imx-gst1.0-plugin', 'c',
  version : '4.7.1',
  meson_version : '>= 0.48',
)

api_version = '1.0'

gst_req = '>= 1.6.0'

pro_version = meson.project_version()
pro_name = meson.project_name()

plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')

unneeded_dep = dependency('', required : false)
if unneeded_dep.found()
  error ('Found unfindable dependency')
endif

gst_dep = dependency('gstreamer-' + api_version, version : gst_req)

#meson automaticaly add _FILE_OFFSET_BITS=64 to gcc, this will make ipu test
#case fail when do mmap on 6q, autoconf didn't add this define, so undef it
add_project_arguments('-U_FILE_OFFSET_BITS', language: 'c')

#below two marcos are defined in autoconf
add_project_arguments ('-DARM', language: 'c')
add_project_arguments ('-D_GUNC_', language: 'c')

build_MX6 = 'no'
build_MX6QP = 'no'
build_MX6SL = 'no'
build_MX6SLL = 'no'
build_MX6SX = 'no'
build_MX6UL = 'no'
build_MX7D = 'no'
build_MX7ULP = 'no'
build_MX8 = 'no'
build_MX9 = 'no'
platforms = get_option('platform')
platform_is_aarch64 = 'no'
foreach platform : platforms
  if platform == 'MX6'
    build_MX6 = 'yes'
  elif platform == 'MX6QP'
    build_MX6QP = 'yes'
  elif platform == 'MX6SL'
    build_MX6SL = 'yes'
  elif platform == 'MX6SLL'
    build_MX6SLL = 'yes'
  elif platform == 'MX6SX'
    build_MX6SX = 'yes'
  elif platform == 'MX6UL'
    build_MX6UL = 'yes'
  elif platform == 'MX7D'
    build_MX7D = 'yes'
  elif platform == 'MX7ULP'
    build_MX7ULP = 'yes'
  elif platform == 'MX8'
    build_MX8 = 'yes'
    platform_is_aarch64 = 'yes'
  elif platform == 'MX9'
    build_MX9 = 'yes'
    platform_is_aarch64 = 'yes'
  else
    error('Unsupported platform provided :' + platform)
  endif
  message('build plugins for ' + platform)
endforeach

build_ipu = false
build_g2d = false
build_pxp = false
if build_MX6 == 'yes' or build_MX6QP == 'yes'
  build_ipu = true
  build_g2d = true
  build_pxp = true
  #HAVE_DEVICE_PXP is enabled for consolidate rootfs when PLATFORM=MX6 for 6sx
endif

if build_MX6SL == 'yes' or build_MX6SX == 'yes'
  build_g2d = true
  build_pxp = true
endif

if build_MX6UL == 'yes' or build_MX7D == 'yes' or build_MX6SLL == 'yes' or build_MX8 == 'yes' or build_MX9 == 'yes'
  build_pxp = true
endif

if build_MX8 == 'yes' or build_MX7ULP == 'yes'
  build_g2d = true
endif

gst_base_dep = dependency('gstreamer-base-' + api_version, version : gst_req)
gst_allocator_dep = dependency('gstreamer-allocators-' + api_version, version : gst_req)
gst_plugins_base_dep = dependency('gstreamer-plugins-base-' + api_version, version : gst_req)
gst_video_dep = dependency('gstreamer-video-' + api_version, version : gst_req)
gst_bad_base_dep = dependency('gstreamer-bad-base-' + api_version, version : '<= 1.14.0', required: false)
gst_bad_video_dep = dependency('gstreamer-bad-video-' + api_version, version : '<= 1.14.0', required: false)
gst_bad_allocator_dep = dependency('gstreamer-bad-allocators-' + api_version, version : '<= 1.14.0', required: false)

if not gst_bad_base_dep.found()
  have_bad_base = false
else
  have_bad_base = true
endif

if not gst_bad_video_dep.found()
  have_bad_video = false
else
  have_bad_video = true
endif

if not gst_bad_allocator_dep.found()
  have_bad_allocator = false
  ionallocator_dep = gst_allocator_dep
else
  have_bad_allocator = true
  ionallocator_dep = gst_bad_allocator_dep
endif

ionallocator_flags = []
if cc.has_header('linux/ion.h') and cc.has_header('gst/allocators/gstionmemory.h', dependencies: ionallocator_dep)
  ionallocator_flags += ['-DUSE_ION']
endif

dmabufheapsallocator_flags = []
if cc.has_header('linux/dma-heap.h') and cc.has_header('gst/allocators/gstdmabufheaps.h', dependencies: gst_allocator_dep)
  dmabufheapsallocator_flags += ['-DUSE_DMABUFHEAPS']
endif

#gst_plugin_ldflags=['-module', '-avoid-version', '-export-symbols-regex']
gst_plugin_ldflags=[]
plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')
libs_install_dir = get_option('libdir')
version_flags = ['-DVERSION="@0@"'.format(pro_version), '-DPACKAGE="@0@"'.format(pro_name)]

subdir('libs')
subdir('plugins')
subdir('tools')

extinc = include_directories('ext-includes')
libsinc = include_directories('libs')

pkgconf = configuration_data()

pkgconf.set('prefix', join_paths(get_option('prefix')))
pkgconf.set('exec_prefix', '${prefix}')
pkgconf.set('libdir', '${exec_prefix}/@0@'.format(get_option('libdir')))
pkgconf.set('includedir', '${prefix}/@0@'.format(get_option('includedir')))
pkgconf.set('pluginsdir', '${exec_prefix}/lib/gstreamer-@0@'.format(api_version))
pkgconf.set('GST_MAJORMINOR', api_version)
pkgconf.set('VERSION', pro_version)

pkg_install_dir = '@0@/pkgconfig'.format(get_option('libdir'))

pkg_files = [
  'gstreamer-fsl',
]

foreach p : pkg_files
  infile = p + '.pc.in'
  outfile = p + '-1.0.pc'
  configure_file(input : infile,
    output : outfile,
    configuration : pkgconf,
    install_dir : pkg_install_dir)
endforeach

license_files = ['COPYING-LGPL-2', 'COPYING-LGPL-2.1']
install_data(sources: license_files)
