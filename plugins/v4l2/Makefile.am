plugin_LTLIBRARIES = libgstimxv4l2.la

libgstimxv4l2_la_SOURCES = gstimxv4l2plugins.c \
				  gstimxv4l2allocator.c \
				  gstimxv4l2sink.c \
				  gstimxv4l2src.c 
				  
libgstimxv4l2_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
				 $(GST_BASE_CFLAGS) \
				 $(GST_CFLAGS) \
				 -I$(top_srcdir)/libs \
				 -I$(top_srcdir)/libs/allocator \
				 -I$(top_srcdir)/libs/v4l2_core \
				 -I$(top_srcdir)/libs/device-2d \
				 -I$(top_srcdir)/libs/overlaycompositionmeta \
				 -I$(top_srcdir)/libs/video-overlay
				 
libgstimxv4l2_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la

if USE_BAD_ALLOCATOR
libgstimxv4l2_la_LIBADD += -lgstbadallocators-$(GST_API_VERSION)
endif
				 
libgstimxv4l2_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxv4l2_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstimxv4l2allocator.h gstimxv4l2sink.h gstimxv4l2src.h
