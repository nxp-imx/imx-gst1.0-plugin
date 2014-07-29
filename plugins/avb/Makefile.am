# plugindir is set in configure
plugin_LTLIBRARIES = libgstavb.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libgstavb_la_SOURCES =  gstavbplugin.c gstavbsink.c gstavbpcmsink.c gstavbmpegtssink.c gstavbsrc.c gstavbpcmsrc.c gstavbmpegtssrc.c avtp.c cip.c ethernet.c gstavbclock.c
libgstavb_la_CFLAGS =  $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) -I$(top_srcdir)/libs -I$(top_srcdir)/ext-includes
libgstavb_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_NET_LIBS) $(GIO_LIBS) -lgstaudio-$(GST_API_VERSION) 
libgstavb_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS)
libgstavb_la_LIBADD += ../../libs/libgstfsl-@GST_API_VERSION@.la
libgstavb_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstavb_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libgstavb_la_CFLAGS += -O2 -fno-omit-frame-pointer -D_ARM12
noinst_HEADERS = gstavbsink.h gstavbpcmsink.h gstavbmpegtssink.h gstavbsrc.h gstavbpcmsrc.h gstavbmpegtssrc.h avtp.h cip.h ethernet.h gstavbclock.h
