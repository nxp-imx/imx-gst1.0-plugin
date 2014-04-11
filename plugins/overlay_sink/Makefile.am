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
				 -I../../libs \
				 -I../../libs/allocator \
				 -I../../libs/gstsutils \
				 -I../../libs/v4l2_core

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


