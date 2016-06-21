plugin_LTLIBRARIES = libgstimxcompositor.la

if PLATFORM_IS_MX8
libgstimxcompositor_la_SOURCES = gstimxcompositor.c \
                                 gstimxcompositorpad.c
else
libgstimxcompositor_la_SOURCES = base/gstaggregator.c \
                                 base/gstvideoaggregator.c \
                                 gstimxcompositor.c \
                                 gstimxcompositorpad.c
endif

libgstimxcompositor_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
				 $(GST_BASE_CFLAGS) \
				 $(GST_CFLAGS) \
				 -I$(top_srcdir)/libs \
				 -I$(top_srcdir)/libs/allocator \
				 -I$(top_srcdir)/libs/device-2d \
				 -I$(top_srcdir)/libs/overlaycompositionmeta
				 
if PLATFORM_IS_MX8
libgstimxcompositor_la_CFLAGS += -DUSE_BAD_GIT -DUSE_DMA_FD 

libgstimxcompositor_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 -lgstbadbase-$(GST_API_VERSION) \
				 -lgstbadvideo-$(GST_API_VERSION) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la
else
libgstimxcompositor_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la
endif

libgstimxcompositor_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxcompositor_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstimxcompositor.h gstimxcompositorpad.h \
                 base/gstaggregator.h base/gstvideoaggregator.h \
                 base/gstvideoaggregatorpad.h
