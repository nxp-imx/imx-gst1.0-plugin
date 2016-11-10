lib_LTLIBRARIES =
bin_PROGRAMS = gplay-@GST_API_VERSION@
gplay_@GST_API_VERSION@_SOURCES = gplay2.c playlist.c
gplay_@GST_API_VERSION@_CFLAGS  = $(GST_CFLAGS)
gplay_@GST_API_VERSION@_LDADD   = ../../libs/libgstfsl-@GST_API_VERSION@.la -lgstplayer-$(GST_API_VERSION) $(GST_LIBS) \
                        ../../libs/libgstfsl-@GST_API_VERSION@.la

noinst_HEADERS = playlist.h
