lib_LTLIBRARIES = libgplaycore-@GST_API_VERSION@.la

libgplaycore_@GST_API_VERSION@_la_SOURCES = mfw_gplay_core.c fsl_player_debug.c fsl_player_osal.c fsl_player_queue.c fsl_player_ui_message.c gst_snapshot.c
libgplaycore_@GST_API_VERSION@_la_CFLAGS  = $(GST_CFLAGS) 
libgplaycore_@GST_API_VERSION@_la_LIBADD = $(GST_LIBS) -lz

bin_PROGRAMS = gplay-@GST_API_VERSION@
gplay_@GST_API_VERSION@_SOURCES = mfw_gplay.c playlist.c
gplay_@GST_API_VERSION@_CFLAGS  = $(GST_CFLAGS) 
gplay_@GST_API_VERSION@_LDADD   = libgplaycore-@GST_API_VERSION@.la

noinst_HEADERS = \
    fsl_player_debug.h      \
    fsl_player_drm_types.h  \
    fsl_player_osal.h       \
    fsl_player_queue.h      \
    fsl_player_types.h      \
    fsl_player_ui_message.h \
    gst_snapshot.h          \
    mfw_gplay_core.h        \
    playlist.h
