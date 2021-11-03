# plugindir is set in configure
CORELIB=spdifparser

plugin_LTLIBRARIES = libgstspdifdemux.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libgstspdifdemux_la_SOURCES =  gstspdifdemux.c
libgstspdifdemux_la_CFLAGS =  $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) -I$(top_srcdir)/libs -I$(top_srcdir)/ext-includes
libgstspdifdemux_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) -lgsttag-$(GST_API_VERSION)
libgstspdifdemux_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstspdifdemux_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libgstimxmp3enc_la_CFLAGS += -O2 -fno-omit-frame-pointer -D_ARM12

# headers we need but don't want installed
noinst_HEADERS =  gstspdifdemux.h