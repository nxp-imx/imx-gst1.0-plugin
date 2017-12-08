ACLOCAL_AMFLAGS = -I m4

LIBSDIRS = libs

if USE_AIUR
AIURDIRS = plugins/aiurdemux
endif

if USE_BEEP
BEEPDIRS = plugins/beepdec
endif

if USE_MP3_ENC
MP3ENC_DIR =  plugins/mp3enc
endif

if USE_WMA8_ENC
WMA8ENC_DIR =
endif

AVBDIRS = plugins/avb

VIDEO_CONVERT_DIRS = plugins/videoconvert

if HAVE_GST_CHECK
COMPOSITOR_DIRS = plugins/compositor
endif

TOOLDIRS =    tools/                      \
              tools/grecorder             \
              tools/gplay2

BASEDIRS = $(AIURDIRS) $(BEEPDIRS) $(VIDEO_CONVERT_DIRS) $(COMPOSITOR_DIRS)
              
if PLATFORM_IS_MX6

if USE_V4L_SINK
V4LSINKDIRS = plugins/v4l2
endif

if USE_OVERLAY_SINK
OVERLAYSINKDIRS = plugins/overlay_sink
endif

if USE_VPU_WRAP
VPUWRAPDIRS = plugins/vpu
endif

endif

if PLATFORM_IS_MX6QP

if USE_V4L_SINK
V4LSINKDIRS = plugins/v4l2
endif

if USE_OVERLAY_SINK
OVERLAYSINKDIRS = plugins/overlay_sink
endif

if USE_VPU_WRAP
VPUWRAPDIRS = plugins/vpu
endif

endif

if PLATFORM_IS_MX6SL

if USE_V4L_SINK
V4LSINKDIRS = plugins/v4l2
endif

if USE_OVERLAY_SINK
OVERLAYSINKDIRS = plugins/overlay_sink
endif

VPUWRAPDIRS =

endif

if PLATFORM_IS_MX6SX

if USE_V4L_SINK
V4LSINKDIRS = plugins/v4l2
endif

if USE_OVERLAY_SINK
OVERLAYSINKDIRS = plugins/overlay_sink
endif

VPUWRAPDIRS =

endif

if PLATFORM_IS_MX7D

if USE_V4L_SINK
V4LSINKDIRS = plugins/v4l2
endif

OVERLAYSINKDIRS =

VPUWRAPDIRS =

endif

if PLATFORM_IS_MX6UL

if USE_V4L_SINK
V4LSINKDIRS = plugins/v4l2
endif

OVERLAYSINKDIRS =

VPUWRAPDIRS =

endif

if PLATFORM_IS_MX6SLL

if USE_V4L_SINK
V4LSINKDIRS = plugins/v4l2
endif

OVERLAYSINKDIRS =

VPUWRAPDIRS =

endif

if PLATFORM_IS_MX8

V4LSINKDIRS =

OVERLAYSINKDIRS =

if USE_VPU_WRAP
VPUWRAPDIRS = plugins/vpu
endif

endif

if PLATFORM_IS_MX7ULP
V4LSINKDIRS =

if USE_OVERLAY_SINK
OVERLAYSINKDIRS = plugins/overlay_sink
endif

VPUWRAPDIRS =

endif

FBDEVSINKDIRS =

SUBDIRS =  $(LIBSDIRS) $(BASEDIRS) $(V4LSINKDIRS) $(OVERLAYSINKDIRS) $(FBDEVSINKDIRS) $(VPUWRAPDIRS) $(WMA8ENC_DIR) $(MP3ENC_DIR) $(TOOLDIRS)




noinst_HEADERS = \
    ext-includes/fsl_media_types.h \
    ext-includes/fsl_types.h			 \
    ext-includes/fsl_parser.h			 \
    ext-includes/vpu_wrapper.h		 \
    ext-includes/fsl_unia.h        \
    ext-includes/mp3_enc_interface.h	  
    

fslmmdocdir = $(docdir)/imx-mm/gstreamer-1.0
fslmmdoc_DATA = 

pcverfiles = gstreamer-fsl-@GST_MAJORMINOR@.pc
### how to generate versioned .pc files from .pc files in this dir
%-@GST_MAJORMINOR@.pc: %.pc
	cp $< $@
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcverfiles)

CLEANFILES = $(pcverfiles)
pcinfiles = gstreamer-fsl.pc.in

DISTCLEANFILES = $(pcinfiles:.in=)

EXTRA_DIST = autogen.sh $(fslmmdoc_DATA) $(pcinfiles) COPYING-LGPL-2 COPYING-LGPL-2.1
