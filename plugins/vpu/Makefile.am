plugin_LTLIBRARIES = libgstvpu.la

noinst_HEADERS = \
	gstvpudec.h \
	gstvpudecobject.h \
	gstvpuallocator.h \
	#gstvpuenc.h

libgstvpu_la_SOURCES = \
	gstvpu.c \
	gstvpudec.c \
	gstvpudecobject.c \
	gstvpuallocator.c \
	#gstvpuenc.c

libgstvpu_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstvpu_la_CFLAGS += -I../../libs -I../../ext-includes
libgstvpu_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) -lgstvideo-$(GST_API_VERSION) \
	$(VPU_LIBS) $(LIBM)
libgstvpu_la_LIBADD += ../../libs/libgstfsl-@GST_API_VERSION@.la

libgstvpu_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstvpu_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

