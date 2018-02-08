lib_LTLIBRARIES = libgstfsl-@GST_API_VERSION@.la

libgstfsl_@GST_API_VERSION@includedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/imx-mm/

if USE_V4L2_CORE
V4L2_CORE_SOURCE = v4l2_core/gstimxv4l2.c
endif
libgstfsl_@GST_API_VERSION@_la_SOURCES = video-tsm/mfw_gst_ts.c \
  gstsutils/gstsutils.c \
	device-2d/imx_2d_device.c \
	device-2d/imx_2d_device_allocator.c \
	overlaycompositionmeta/imxoverlaycompositionmeta.c \
	video-overlay/gstimxvideooverlay.c \
	gstimxcommon.c \
  $(V4L2_CORE_SOURCE)

libgstfsl_@GST_API_VERSION@_la_CFLAGS = $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstfsl_@GST_API_VERSION@_la_LIBADD = $(GST_BASE_LIBS) -lgstallocators-$(GST_API_VERSION)
libgstfsl_@GST_API_VERSION@_la_LDFLAGS = -lgstvideo-$(GST_API_VERSION)
libgstfsl_@GST_API_VERSION@_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = video-tsm/mfw_gst_ts.h \
    gstsutils/gstsutils.h \
    v4l2_core/gstimxv4l2.h \
    device-2d/imx_2d_device.h \
    device-2d/imx_2d_device_allocator.h \
    overlaycompositionmeta/imxoverlaycompositionmeta.h \
    video-overlay/gstimxvideooverlay.h

if USE_BAD_ALLOCATOR
libgstfsl_@GST_API_VERSION@_la_LIBADD += -lgstbadallocators-$(GST_API_VERSION)
endif

if USE_ION
libgstfsl_@GST_API_VERSION@_la_CFLAGS += -DUSE_ION
endif

if USE_X11
libgstfsl_@GST_API_VERSION@_la_SOURCES += video-overlay/gstimxxoverlay.c
libgstfsl_@GST_API_VERSION@_la_LIBADD += -lX11
libgstfsl_@GST_API_VERSION@_la_CFLAGS += -DUSE_X11    
noinst_HEADERS += video-overlay/gstimxxoverlay.h
endif
				 
if USE_IMX_2DDEVICE_IPU
libgstfsl_@GST_API_VERSION@_la_SOURCES += device-2d/imx_2d_device_ipu.c
libgstfsl_@GST_API_VERSION@_la_CFLAGS += -DUSE_IPU
endif

if USE_IMX_2DDEVICE_G2D
libgstfsl_@GST_API_VERSION@_la_SOURCES += device-2d/imx_2d_device_g2d.c
libgstfsl_@GST_API_VERSION@_la_CFLAGS += -DUSE_G2D
libgstfsl_@GST_API_VERSION@_la_LIBADD += -lg2d
endif

if USE_IMX_2DDEVICE_PXP
libgstfsl_@GST_API_VERSION@_la_SOURCES += device-2d/imx_2d_device_pxp.c
libgstfsl_@GST_API_VERSION@_la_CFLAGS += -DUSE_PXP
libgstfsl_@GST_API_VERSION@_la_LIBADD += -lpxp
endif
