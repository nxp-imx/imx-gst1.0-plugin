# plugindir is set in configure
CORELIB=_mp3_enc_arm12_elinux

plugin_LTLIBRARIES = libgstimxmp3enc.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libgstimxmp3enc_la_SOURCES =  gstimxmp3.c gstimxmp3enc.c
libgstimxmp3enc_la_CFLAGS =  $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) -I$(top_srcdir)/libs -I$(top_srcdir)/ext-includes
libgstimxmp3enc_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) -lgstaudio-$(GST_API_VERSION) -lgsttag-$(GST_API_VERSION) -lgstriff-$(GST_API_VERSION)
libgstimxmp3enc_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS) 
libgstimxmp3enc_la_LIBADD += ../../libs/libgstfsl-@GST_API_VERSION@.la -l$(CORELIB)
libgstimxmp3enc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstimxmp3enc_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
libgstimxmp3enc_la_CFLAGS += -O2 -fno-omit-frame-pointer -D_ARM12

# headers we need but don't want installed
noinst_HEADERS =  gstimxmp3enc.h