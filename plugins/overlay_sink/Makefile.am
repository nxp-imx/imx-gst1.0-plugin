plugin_LTLIBRARIES = libgstoverlaysinkplugins.la

libgstoverlaysinkplugins_la_SOURCES = gstosink.c \
				      gstosinkallocator.c \
				      osink_object.c \
				      compositor.c \
				      device.c \
				      displaysv4l2.c

libgstoverlaysinkplugins_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
				 $(GST_BASE_CFLAGS) \
				 $(GST_CFLAGS) \
				 -I$(top_srcdir)/libs \
				 -I$(top_srcdir)/libs/allocator \
				 -I$(top_srcdir)/libs/gstsutils \
				 -I$(top_srcdir)/libs/v4l2_core \
                                 -I$(top_srcdir)/ext-includes

libgstoverlaysinkplugins_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 -lg2d \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la

libgstoverlaysinkplugins_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstoverlaysinkplugins_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = osink_common.h gstosink.h gstosinkallocator.h osink_object.h compositor.h device.h displays.h

registry_file1 = imx_6q_display_config
registry_file2 = imx_6sx_display_config

if PLATFORM_IS_MX6
data_DATA = $(registry_file1)
endif

if PLATFORM_IS_MX6SX
data_DATA = $(registry_file2)
endif

EXTRA_DIST = $(registry_file1) $(registry_file2)

if USE_X11
libgstoverlaysinkplugins_la_CFLAGS += -DUSE_X11
libgstoverlaysinkplugins_la_CFLAGS += -I$(top_srcdir)/libs/video-overlay
endif
