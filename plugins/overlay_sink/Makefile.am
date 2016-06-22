plugin_LTLIBRARIES = libgstoverlaysinkplugins.la

if PLATFORM_IS_MX8
libgstoverlaysinkplugins_la_SOURCES = gstosink.c \
              gstosinkallocator.c \
				      osink_object.c \
				      compositor.c \
				      displaysfb.c
else
libgstoverlaysinkplugins_la_SOURCES = gstosink.c \
              gstosinkallocator.c \
				      osink_object.c \
				      compositor.c \
				      displaysv4l2.c
endif

libgstoverlaysinkplugins_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
				 $(GST_BASE_CFLAGS) \
				 $(GST_CFLAGS) \
				 -I$(top_srcdir)/libs \
				 -I$(top_srcdir)/libs/allocator \
				 -I$(top_srcdir)/libs/gstsutils \
				 -I$(top_srcdir)/libs/v4l2_core \
				 -I$(top_srcdir)/libs/device-2d \
				 -I$(top_srcdir)/libs/overlaycompositionmeta \
         -I$(top_srcdir)/ext-includes \
         -I$(top_srcdir)/libs/video-overlay

if PLATFORM_IS_MX8
libgstoverlaysinkplugins_la_CFLAGS += -DUSE_DMA_FD -DUSE_FB_API
endif

libgstoverlaysinkplugins_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 -lg2d \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la

libgstoverlaysinkplugins_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstoverlaysinkplugins_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = osink_common.h gstosink.h gstosinkallocator.h osink_object.h compositor.h displays.h

registry_file1 = imx_6q_display_config
registry_file2 = imx_6sx_display_config
registry_file3 = imx_8dv_display_config

data_DATA = $(registry_file1) $(registry_file2) $(registry_file3)
EXTRA_DIST = $(registry_file1) $(registry_file2) $(registry_file3)
