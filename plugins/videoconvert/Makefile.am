plugin_LTLIBRARIES = libgstimxvideoconvert.la

libgstimxvideoconvert_la_SOURCES = videoprocessdevice.c \
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
				 ../../libs/libgstfsl-@GST_API_VERSION@.la
				 
if USE_IPU
libgstimxvideoconvert_la_SOURCES += videoprocessdevice_ipu.c
libgstimxvideoconvert_la_CFLAGS += -DUSE_IPU
endif

if USE_G2D
libgstimxvideoconvert_la_SOURCES += videoprocessdevice_g2d.c
libgstimxvideoconvert_la_CFLAGS += -DUSE_G2D
libgstimxvideoconvert_la_LIBADD += -lg2d
endif

libgstimxvideoconvert_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxvideoconvert_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = videoprocessdevice.h gstimxvideoconvert.h gstimxvideoconvertallocator.h
