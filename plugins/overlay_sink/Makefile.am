plugin_LTLIBRARIES = libgstoverlaysink.la

libgstoverlaysink_la_SOURCES = gstosink.c \
              gstosinkallocator.c \
				      osink_object.c \
				      compositor.c

libgstoverlaysink_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
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

if USE_FB_DISPLAY
libgstoverlaysink_la_SOURCES += displaysfb.c
else
libgstoverlaysink_la_SOURCES += displaysv4l2.c
endif

if PLATFORM_IS_MX8
libgstoverlaysink_la_CFLAGS += -DUSE_FB_API
endif

if USE_ION
libgstoverlaysink_la_CFLAGS += -DUSE_ION
endif

libgstoverlaysink_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 -lg2d \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la

if USE_BAD_ALLOCATOR
libgstoverlaysink_la_LIBADD += -lgstbadallocators-$(GST_API_VERSION)
endif

libgstoverlaysink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstoverlaysink_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = osink_common.h gstosink.h gstosinkallocator.h osink_object.h compositor.h displays.h

registry_file1 = imx_6q_display_config
registry_file2 = imx_6sx_display_config
registry_file3 = imx_8dv_display_config
registry_file4 = imx_7ulp_display_config

data_DATA = $(registry_file1) $(registry_file2) $(registry_file3) $(registry_file4)
EXTRA_DIST = $(registry_file1) $(registry_file2) $(registry_file3) $(registry_file4)
