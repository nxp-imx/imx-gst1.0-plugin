lib_LTLIBRARIES = libgstfsl-@GST_API_VERSION@.la

libgstfsl_@GST_API_VERSION@_la_SOURCES = allocator/gstallocatorphymem.c \
	video-tsm/mfw_gst_ts.c \
        gstsutils/gstsutils.c \
	v4l2_core/gstimxv4l2.c \
        gstimxcommon.h

libgstfsl_@GST_API_VERSION@_la_CFLAGS = $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstfsl_@GST_API_VERSION@_la_LIBADD = $(GST_BASE_LIBS)
libgstfsl_@GST_API_VERSION@_la_LDFLAGS = -lgstvideo-$(GST_API_VERSION)
libgstfsl_@GST_API_VERSION@_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = allocator/gstallocatorphymem.h \
    video-tsm/mfw_gst_ts.h \
    gstsutils/gstsutils.h \
    v4l2_core/gstimxv4l2.h
