plugin_LTLIBRARIES = libgstimxvideoconvert.la

libgstimxvideoconvert_la_SOURCES = gstimxvideoconvert.c

libgstimxvideoconvert_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
				 $(GST_BASE_CFLAGS) \
				 $(GST_CFLAGS) \
				 -I$(top_srcdir)/libs \
				 -I$(top_srcdir)/libs/allocator \
				 -I$(top_srcdir)/libs/device-2d \
				 -I$(top_srcdir)/libs/overlaycompositionmeta

if USE_ION
libgstimxvideoconvert_la_CFLAGS += -DUSE_ION
endif

libgstimxvideoconvert_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la

libgstimxvideoconvert_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxvideoconvert_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstimxvideoconvert.h
