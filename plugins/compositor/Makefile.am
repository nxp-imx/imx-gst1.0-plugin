plugin_LTLIBRARIES = libgstimxcompositor.la

libgstimxcompositor_la_SOURCES = gstimxcompositor.c \
                                 gstimxcompositorpad.c

libgstimxcompositor_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
				 $(GST_BASE_CFLAGS) \
				 $(GST_CFLAGS) \
				 -I$(top_srcdir)/libs \
				 -I$(top_srcdir)/libs/allocator \
				 -I$(top_srcdir)/libs/device-2d \
				 -I$(top_srcdir)/libs/overlaycompositionmeta
				 
if USE_ION
libgstimxcompositor_la_CFLAGS += -DUSE_ION
endif

libgstimxcompositor_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 -lgstbadvideo-$(GST_API_VERSION) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la

if USE_BAD_BASE
libgstimxcompositor_la_LIBADD += -lgstbadbase-$(GST_API_VERSION)
endif

libgstimxcompositor_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxcompositor_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstimxcompositor.h gstimxcompositorpad.h
