extinc = include_directories('../ext-includes')
libsinc = include_directories('../libs')

python3 = import('python').find_installation()
sys_root = run_command(python3, '-c', 'import os; print(os.environ.get("PKG_CONFIG_SYSROOT_DIR"))').stdout().strip()

gsttag_dep = cc.find_library('gsttag-' + api_version)
gstriff_dep = cc.find_library('gstriff-' + api_version)
vpuwrap_dep = dependency('libfslvpuwrap', required: false)

subdir('compositor')
subdir('videoconvert')

imx_parser_dep = dependency('imx-parser', required: false)
if cc.has_header('fsl_parser.h', dependencies: imx_parser_dep)
  subdir('aiurdemux')
endif

fslaudiocodec_dep = dependency('libfslaudiocodec', required: false)
if cc.has_header('fsl_unia.h', dependencies: fslaudiocodec_dep)
  subdir('beepdec')
endif

if cc.has_header('mp3_enc_interface.h', dependencies: fslaudiocodec_dep)
  subdir('mp3enc')
endif

have_g2d = false
have_vpuwrapper = false
have_v4l2 = false
if cc.has_header('g2d.h')
  have_g2d = true
endif
if cc.has_header('vpu_wrapper.h', dependencies: vpuwrap_dep)
  have_vpuwrapper = true
endif
if cc.has_header('linux/videodev2.h')
  have_v4l2 = true
endif

if build_MX6 == 'yes'
  if have_v4l2
    subdir('v4l2')
  endif
  if have_g2d
    subdir('overlay_sink')
  endif
  if have_vpuwrapper
    subdir('vpu')
  endif
elif build_MX6QP == 'yes'
  if have_v4l2
    subdir('v4l2')
  endif
  if have_g2d
    subdir('overlay_sink')
  endif
  if have_vpuwrapper
    subdir('vpu')
  endif
elif build_MX6SL == 'yes'
  if have_v4l2
    subdir('v4l2')
  endif
  if have_g2d
    subdir('overlay_sink')
  endif
elif build_MX6SLL == 'yes'
  if have_v4l2
    subdir('v4l2')
  endif
elif build_MX6SX == 'yes'
  if have_v4l2
    subdir('v4l2')
  endif
  if have_g2d
    subdir('overlay_sink')
  endif
elif build_MX6UL == 'yes'
  if have_v4l2
    subdir('v4l2')
  endif
elif build_MX7D == 'yes'
  if have_v4l2
    subdir('v4l2')
  endif
elif build_MX7ULP == 'yes'
  if have_g2d
    subdir('overlay_sink')
  endif
elif build_MX8 == 'yes' or build_MX9 == 'yes'
  subdir('spdifdemux')
  if have_vpuwrapper
    subdir('vpu')
  endif
endif