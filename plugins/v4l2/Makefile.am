plugin_LTLIBRARIES = libgstimxv4l2plugins.la

libgstimxv4l2plugins_la_SOURCES = gstimxv4l2plugins.c \
				  gstimxv4l2.c \
				  gstimxv4l2allocator.c \
				  gstimxv4l2sink.c \
				  gstimxv4l2src.c 
libgstimxv4l2plugins_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
				 $(GST_BASE_CFLAGS) \
				 $(GST_CFLAGS) \
				 -I../../libs \
				 -I../../libs/allocator
libgstimxv4l2plugins_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la
libgstimxv4l2plugins_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxv4l2plugins_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstimxv4l2.h gstimxv4l2allocator.h gstimxv4l2sink.h gstimxv4l2src.h



