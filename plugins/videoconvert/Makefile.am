plugin_LTLIBRARIES = libgstimxvideoconvert.la

libgstimxvideoconvert_la_SOURCES = videoprocessdevice_g2d.c \
                                   videoprocessdevice_ipu.c \
                                   videoprocessdevice.c \
                                   gstimxvideoconvertallocator.c \
                                   gstimxvideoconvert.c

libgstimxvideoconvert_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) \
				 $(GST_BASE_CFLAGS) \
				 $(GST_CFLAGS) \
				 -I$(top_srcdir)/libs \
				 -I$(top_srcdir)/libs/allocator
libgstimxvideoconvert_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
				 $(GST_BASE_LIBS) \
				 $(GST_PLUGINS_BASE_LIBS) \
				 -lgstvideo-$(GST_API_VERSION) \
				 $(GST_LIBS) \
				 -lg2d \
				 ../../libs/libgstfsl-@GST_API_VERSION@.la
				 
libgstimxvideoconvert_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxvideoconvert_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = videoprocessdevice.h gstimxvideoconvert.h gstimxvideoconvertallocator.h
