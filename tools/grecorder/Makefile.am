lib_LTLIBRARIES = librecorder_engine-@GST_API_VERSION@.la

librecorder_engine_@GST_API_VERSION@includedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/imx-mm/recorder_engine
librecorder_engine_@GST_API_VERSION@include_HEADERS = fsl_types.h recorder_engine.h
librecorder_engine_@GST_API_VERSION@_la_SOURCES = recorder_engine.c
librecorder_engine_@GST_API_VERSION@_la_CFLAGS  = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS)
librecorder_engine_@GST_API_VERSION@_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) \
	-lgstpbutils-@GST_API_VERSION@ \
	-lgstvideo-@GST_API_VERSION@

bin_PROGRAMS = grecorder-@GST_API_VERSION@
grecorder_@GST_API_VERSION@_SOURCES = grecorder.c
grecorder_@GST_API_VERSION@_CFLAGS  = $(GST_CFLAGS) 
grecorder_@GST_API_VERSION@_LDADD   = librecorder_engine-@GST_API_VERSION@.la

opencv_haarcascadesdir = $(pkgdatadir)/$(GST_API_VERSION)/opencv_haarcascades
opencv_haarcascades_DATA = haarcascade_frontalface_old_format.xml

EXTRA_DIST = $(opencv_haarcascades_DATA)

