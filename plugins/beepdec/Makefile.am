# plugindir is set in configure
plugin_LTLIBRARIES = libgstbeepdec.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libgstbeepdec_la_SOURCES =  beep.c beepregistry.c beepdec.c beeptypefind.c
libgstbeepdec_la_CFLAGS =  $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) -I$(top_srcdir)/libs -I$(top_srcdir)/ext-includes -O2 -fno-omit-frame-pointer
libgstbeepdec_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) -lgstaudio-$(GST_API_VERSION) -lgsttag-$(GST_API_VERSION) -lgstriff-$(GST_API_VERSION)
libgstbeepdec_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS) 
libgstbeepdec_la_LIBADD += ../../libs/libgstfsl-@GST_API_VERSION@.la
libgstbeepdec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstbeepdec_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
if PLATFORM_IS_ARCH32
libgstbeepdec_la_CFLAGS += -D_ARM12
registry_file=beep_registry_1.0.arm12.cf.in
reg_inst_file=beep_registry_1.0.arm12.cf
else
registry_file=beep_registry_1.0.arm.cf.in
reg_inst_file=beep_registry_1.0.arm.cf
endif

# headers we need but don't want installed
noinst_HEADERS =  beepregistry.h beepdec.h
data_DATA = $(reg_inst_file)

EXTRA_DIST = $(registry_file)
