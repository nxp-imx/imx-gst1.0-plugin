lib_LTLIBRARIES = libplayengine-@GST_API_VERSION@.la

libplayengine_@GST_API_VERSION@includedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/imx-mm/playengine
libplayengine_@GST_API_VERSION@include_HEADERS = playengine.h
libplayengine_@GST_API_VERSION@_la_SOURCES =  playengine.c
libplayengine_@GST_API_VERSION@_la_CFLAGS  = $(GST_CFLAGS) 
libplayengine_@GST_API_VERSION@_la_LIBADD = $(GST_LIBS) -lz -lgstvideo-$(GST_API_VERSION) -lgstapp-$(GST_API_VERSION) \
  						../../libs/libgstfsl-@GST_API_VERSION@.la	

#bin_PROGRAMS = gplay-@GST_API_VERSION@
#gplay_@GST_API_VERSION@_SOURCES = gplay.c playlist.c
#gplay_@GST_API_VERSION@_CFLAGS  = $(GST_CFLAGS)
#gplay_@GST_API_VERSION@_LDADD   = libplayengine-@GST_API_VERSION@.la

noinst_HEADERS = playlist.h
