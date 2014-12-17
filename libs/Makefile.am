lib_LTLIBRARIES = libgstfsl-@GST_API_VERSION@.la

libgstfsl_@GST_API_VERSION@_la_SOURCES = allocator/gstallocatorphymem.c \
	allocator/gstphymemmeta.c \
	video-tsm/mfw_gst_ts.c \
        gstsutils/gstsutils.c \
	v4l2_core/gstimxv4l2.c \
        gstimxcommon.h

libgstfsl_@GST_API_VERSION@_la_CFLAGS = $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstfsl_@GST_API_VERSION@_la_LIBADD = $(GST_BASE_LIBS)
libgstfsl_@GST_API_VERSION@_la_LDFLAGS = -lgstvideo-$(GST_API_VERSION)
libgstfsl_@GST_API_VERSION@_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = allocator/gstallocatorphymem.h \
	  allocator/gstphymemmeta.h \
    video-tsm/mfw_gst_ts.h \
    gstsutils/gstsutils.h \
    v4l2_core/gstimxv4l2.h

if USE_X11
libgstfsl_@GST_API_VERSION@_la_SOURCES += video-overlay/gstimxvideooverlay.c \
																					video-overlay/gstimxxoverlay.c
libgstfsl_@GST_API_VERSION@_la_LIBADD += -lX11
libgstfsl_@GST_API_VERSION@_la_CFLAGS += -DUSE_X11    
noinst_HEADERS += video-overlay/gstimxvideooverlay.h \
									video-overlay/gstimxxoverlay.h
endif
