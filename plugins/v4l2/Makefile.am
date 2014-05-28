plugin_LTLIBRARIES = libgstimxv4l2plugins.la

libgstimxv4l2plugins_la_SOURCES = gstimxv4l2plugins.c \
				  gstimxv4l2allocator.c \
				  gstimxv4l2sink.c \
				  gstimxv4l2src.c 
libgstimxv4l2plugins_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
				 $(GST_BASE_CFLAGS) \
				 $(GST_CFLAGS) \
				 -I$(top_srcdir)/libs \
				 -I$(top_srcdir)/libs/allocator \
				 -I$(top_srcdir)/libs/v4l2_core
libgstimxv4l2plugins_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 $(top_srcdir)/libs/libgstfsl-@GST_API_VERSION@.la
libgstimxv4l2plugins_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxv4l2plugins_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstimxv4l2allocator.h gstimxv4l2sink.h gstimxv4l2src.h

libgstimxv4l2plugins_la_CFLAGS += -DUSE_X11
libgstimxv4l2plugins_la_CFLAGS += -I$(top_srcdir)/libs/video-overlay


