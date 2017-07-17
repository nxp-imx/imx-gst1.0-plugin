plugin_LTLIBRARIES = libgstimxfbdevsink.la

libgstimxfbdevsink_la_SOURCES = gstimxfbdevsink.c
libgstimxfbdevsink_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	 -I$(top_srcdir)/libs \
	 -I$(top_srcdir)/libs/allocator \
	 -I$(top_srcdir)/libs/gstsutils \
	 -I$(top_srcdir)/ext-includes \
	 -I$(top_srcdir)/libs/video-overlay
libgstimxfbdevsink_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) -lgstvideo-$(GST_API_VERSION) \
	$(GST_BASE_LIBS) \
	$(LIBFBDEV_LIBS) \
	../../libs/libgstfsl-@GST_API_VERSION@.la

if USE_BAD_ALLOCATOR
libgstimxfbdevsink_la_LIBADD += -lgstbadallocators-$(GST_API_VERSION)
endif

if USE_ION
libgstimxfbdevsink_la_CFLAGS += -DUSE_ION
endif

libgstimxfbdevsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxfbdevsink_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstimxfbdevsink.h
