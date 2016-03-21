# plugindir is set in configure
plugin_LTLIBRARIES = libgstaiurdemux.la

# for the next set of variables, rename the prefix if you renamed the .la
# sources used to compile this plug-in
libgstaiurdemux_la_SOURCES =  aiur.c aiurregistry.c aiurstreamcache.c aiuridxtab.c aiurdemux.c aiurtypefind.c aiurcontent.c
libgstaiurdemux_la_CFLAGS =  $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) -I$(top_srcdir)/libs -I$(top_srcdir)/ext-includes
libgstaiurdemux_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(GST_LIBS) -lgsttag-$(GST_API_VERSION) -lgstriff-$(GST_API_VERSION)
libgstaiurdemux_la_CPPFLAGS = $(GST_LIBS_CPPFLAGS) 
libgstaiurdemux_la_LIBADD += ../../libs/libgstfsl-@GST_API_VERSION@.la
libgstaiurdemux_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstaiurdemux_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)


# flags used to compile this plugin
# we use the GST_LIBS flags because we might be using plug-in libs
if PLATFORM_IS_ARCH32
libgstaiurdemux_la_CFLAGS += -D_ARM11
registry_file=aiur_registry_1.0.arm11.cf.in
reg_inst_file=aiur_registry_1.0.arm11.cf
else
registry_file=aiur_registry_1.0.arm.cf.in
reg_inst_file=aiur_registry_1.0.arm.cf
endif

# headers we need but don't want installed
noinst_HEADERS =  aiurregistry.h aiurdemux.h aiurstreamcache.h aiuridxtab.h aiurcontent.h
data_DATA = $(reg_inst_file)

EXTRA_DIST = $(registry_file)
