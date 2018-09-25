AC_PREREQ(2.59)

dnl initialize autoconf
dnl releases only do -Wall, git and prerelease does -Werror too
dnl use a three digit version number for releases, and four for git/prerelease
AC_INIT(GStreamer i.MX Plug-ins, 4.4.4, http://www.freescale.com, imx-gst1.0-plugin)

dnl initialize automake
AM_INIT_AUTOMAKE([foreign])


dnl our libraries and install dirs use GST_API_VERSION in the filename
dnl to allow side-by-side installation of different API versions
GST_API_VERSION=1.0
AC_SUBST(GST_API_VERSION)
AC_DEFINE_UNQUOTED(GST_API_VERSION, "$GST_API_VERSION",
  [GStreamer API Version])

GST_REQ=1.6.0

PKG_CHECK_MODULES(GST, gstreamer-$GST_API_VERSION >= $GST_REQ,
  [HAVE_GST_CHECK="yes"], [HAVE_GST_CHECK="no"
    AC_MSG_NOTICE([no gstreamer-$GST_API_VERSION >= $GST_REQ (GStreamer) found])
  ])

AM_CONDITIONAL(HAVE_GST_CHECK, test "x$HAVE_GST_CHECK" = "xyes")

AC_CONFIG_MACRO_DIR([m4])

AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

AC_DEFUN([FIND_PARSER_SONAME],
[
    AC_MSG_CHECKING([for lib_$1_parser_$2_elinux.3.0.so soname])
    path=`$PKG_CONFIG --variable=libdir imx-parser`
    SONAME=$( $CC -print-file-name=$PKG_CONFIG_SYSROOT_DIR$path/lib_$1_parser_$2_elinux.so | \
	    while read output; do objdump -p $output | \
	    grep "SONAME" | \
	    sed -e 's/ \+SONAME \+//'; done 2> /dev/null )
    if [[ -z "$SONAME" ]]; then
    	AC_MSG_RESULT([no])
	AC_MSG_WARN([Unable to determine soname of lib_$1_parser_$2_elinux.so library, use default])
	SONAME=lib_$1_parser_$2_elinux.so
    else
    	AC_MSG_RESULT([$SONAME])
    fi
    $1_$2_SONAME=$path/$SONAME
    AC_SUBST($1_$2_SONAME)
])

AC_DEFUN([FIND_ACODEC_SONAME],
[
    AC_MSG_CHECKING([for lib_$1_wrap_$2_elinux.so soname])
    path=`$PKG_CONFIG --variable=libdir libfslaudiocodec`
    SONAME=$( $CC -print-file-name=$PKG_CONFIG_SYSROOT_DIR$path/wrap/lib_$1_wrap_$2_elinux.so | \
	    while read output; do objdump -p $output | \
	    grep "SONAME" | \
	    sed -e 's/ \+SONAME \+//'; done 2> /dev/null )
    if [[ -z "$SONAME" ]]; then
    	AC_MSG_RESULT([no])
	AC_MSG_WARN([Unable to determine soname of lib_$1_wrap_$2_elinux.so library, use default])
	SONAME=lib_$1_wrap_$2_elinux.so
    else
    	AC_MSG_RESULT([$SONAME])
    fi

    $1_$2_SONAME=$path/wrap/$SONAME
    AC_SUBST($1_$2_SONAME)
])

AC_DEFUN([CHECK_DISABLE_FEATURE],
[
    AC_ARG_ENABLE($1,
        [AS_HELP_STRING([--disable-[$1]], [$2])],
        [use_[$1]=$enableval],
        [use_[$1]=yes])
    if test "$use_[$1]" = "yes"; then
        for h in "$4"; do
            AC_CHECK_HEADERS([$h], HAVE_[$3]=yes && break, HAVE_[$3]=no)
        done
        if test "x$HAVE_[$3]" = "xyes"; then
            enabled_feature="$enabled_feature\n\t\t[$5]"
        else
            disabled_feature="$disabled_feature\n\t\t[$5]"
        fi
    else
        disabled_feature="$disabled_feature\n\t\t[$5]"
    fi
    AM_CONDITIONAL(USE_[$3], test "x$HAVE_[$3]" = "xyes")
])

AC_DEFUN([CHECK_DISABLE_FEATURE_EX],
[
    AC_ARG_ENABLE($1,
        [AS_HELP_STRING([--disable-[$1]], [$2])],
        [use_[$1]=$enableval],
        [use_[$1]=yes])
    if test "$use_[$1]" = "yes"; then
        for h in "$4"; do
            AC_CHECK_HEADERS([$h], HAVE_[$3]=yes && break, HAVE_[$3]=no)
        done
        if test "x$HAVE_[$3]" = "xyes" && test "x[$6]" = "xyes"; then
            enabled_feature="$enabled_feature\n\t\t[$5]"
        else
            disabled_feature="$disabled_feature\n\t\t[$5]"
        fi
    else
        disabled_feature="$disabled_feature\n\t\t[$5]"
    fi
    AM_CONDITIONAL(USE_[$3], test "x$HAVE_[$3]" = "xyes" && test "x[$6]" = "xyes")
])

dnl AM_MAINTAINER_MODE provides the option to enable maintainer mode
AM_MAINTAINER_MODE

dnl check for tools
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl check target type
if  test "$PLATFORM" = "MX6" || test "$PLATFORM" = "MX6SL" || test "$PLATFORM" = "MX6SX" || test "$PLATFORM" = "MX7D" || test "$PLATFORM" = "MX6UL" || test "$PLATFORM" = "MX6SLL" || test "$PLATFORM" = "MX6QP" || test "$PLATFORM" = "MX8" || test "$PLATFORM" = "MX7ULP";
then
  AC_MSG_NOTICE(build for $PLATFORM)
else
  AC_MSG_ERROR(No target platform specified! Use ./configure PLATFORM=(MX6/MX6QP/MX6SL/MX6SX/MX6UL/MX7D/MX6SLL/MX8/MX7ULP) to retry)
fi

AC_SUBST(PLATFORM)

dnl Now we're ready to ask for gstreamer libs and cflags
dnl And we can also ask for the right version of gstreamer

GST_REQUIRED=1.0

GST_MAJORMINOR=1.0
PKG_CHECK_MODULES(GST, \
  gstreamer-$GST_MAJORMINOR >= $GST_REQUIRED,
  HAVE_GST=yes,HAVE_GST=no)

dnl Give error and exit if we don't have gstreamer
if test "x$HAVE_GST" = "xno"; then
  AC_MSG_ERROR(you need gstreamer development packages installed !)
fi

dnl make GST_CFLAGS and GST_LIBS available
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

dnl make GST_MAJORMINOR available in Makefile.am
AC_SUBST(GST_MAJORMINOR)

dnl If we need them, we can also use the plugin libraries
PKG_CHECK_MODULES(GST_BASE, gstreamer-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_BASE=yes, HAVE_GST_BASE=no)

dnl Give a warning if we don't have gstreamer libs
if test "x$HAVE_GST_BASE" = "xno"; then
  AC_MSG_ERROR(no GStreamer base found)
fi

dnl make GST_BASE_CFLAGS and GST_BASE_LIBS available
AC_SUBST(GST_BASE_CFLAGS)
AC_SUBST(GST_BASE_LIBS)

dnl If we need them, we can also use the plugin libraries
PKG_CHECK_MODULES(GST_PLUGINS_BASE, gstreamer-plugins-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_PLUGINS_BASE=yes, HAVE_GST_PLUGINS_BASE=no)

dnl Give a warning if we don't have gstreamer libs
if test "x$HAVE_GST_PLUGINS_BASE" = "xno"; then
  AC_MSG_ERROR(no GStreamer plugin base libs found)
fi

dnl make GST_PLUGINS_BASE_CFLAGS and GST_PLUGINS_BASE_LIBS available
AC_SUBST(GST_PLUGINS_BASE_CFLAGS)
AC_SUBST(GST_PLUGINS_BASE_LIBS)

if test "x$PLATFORM" = "xMX8" || test "x$PLATFORM" = "xMX8"; then
dnl If we need them, we can also use the plugin libraries
PKG_CHECK_MODULES(GST_BAD_BASE, gstreamer-bad-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_BAD_BASE=yes, HAVE_GST_BAD_BASE=no)

dnl Give a warning if we don't have gstreamer libs
if test "x$HAVE_GST_BAD_BASE" = "xno"; then
  AC_MSG_WARN(no GStreamer bad-base found)
fi

dnl make GST_BAD_BASE_CFLAGS and GST_BAD_BASE_LIBS available
AC_SUBST(GST_BAD_BASE_CFLAGS)
AC_SUBST(GST_BAD_BASE_LIBS)

dnl If we need them, we can also use the plugin libraries
PKG_CHECK_MODULES(GST_BAD_VIDEO, gstreamer-bad-video-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_BAD_VIDEO=yes, HAVE_GST_BAD_VIDEO=no)

dnl Give a warning if we don't have gstreamer libs
if test "x$HAVE_GST_BAD_VIDEO" = "xno"; then
  AC_MSG_ERROR(no GStreamer bad-video found)
fi

dnl make GST_BAD_VIDEO_CFLAGS and GST_BAD_VIDEO_LIBS available
AC_SUBST(GST_BAD_VIDEO_CFLAGS)
AC_SUBST(GST_BAD_VIDEO_LIBS)
fi

dnl check ion allocator headfile
old_CFLAGS=$CFLAGS
PKG_CHECK_MODULES(GST, gstreamer-$GST_MAJORMINOR >= 1.14, CFLAGS="$CFLAGS `$PKG_CONFIG --cflags gstreamer-allocators-1.0`",\
                  CFLAGS="$CFLAGS `$PKG_CONFIG --cflags gstreamer-bad-allocators-1.0`")
AC_CHECK_HEADERS([linux/ion.h gst/allocators/gstionmemory.h], HAVE_ION="yes", HAVE_ION="no")
AM_CONDITIONAL(USE_ION, test "x$HAVE_ION" = "xyes")
CFLAGS=$old_CFLAGS

HAVE_BAD_ALLOCATOR=no
AC_CHECK_LIB(gstbadallocators-$GST_MAJORMINOR, gst_buffer_is_phymem, [HAVE_BAD_ALLOCATOR=yes])
AM_CONDITIONAL(USE_BAD_ALLOCATOR, test "x$HAVE_BAD_ALLOCATOR" = "xyes")

PKG_CHECK_MODULES(GST, gstreamer-$GST_MAJORMINOR >= 1.14, HAVE_BAD_BASE=no, HAVE_BAD_BASE=yes)
AM_CONDITIONAL(USE_BAD_BASE, test "x$HAVE_BAD_BASE" = "xyes")

AC_CHECK_HEADERS([hantro_enc/ewl.h], [HAVE_H1_ENC=yes], [HAVE_H1_ENC=no])
AM_CONDITIONAL(USE_H1_ENC, test "x$HAVE_H1_ENC" = "xyes")

AC_CHECK_HEADERS([vpu_lib.h], [HAVE_VPU_LIB=yes], [HAVE_VPU_LIB=no])
if test "x$HAVE_VPU_LIB" = "xyes"; then
    VPU_LIBS=`$PKG_CONFIG --libs libfslvpuwrap 2>/dev/null`
    VPU_LIBS+=" -lvpu"
fi
AC_CHECK_HEADERS([hantro_dec/dwl.h], [HAVE_VPU_LIB=yes], [HAVE_VPU_LIB=no])
if test "x$HAVE_VPU_LIB" = "xyes"; then
    VPU_LIBS=`$PKG_CONFIG --libs libfslvpuwrap 2>/dev/null`
fi
AC_CHECK_HEADERS([malone/malone_interface.h], [HAVE_VPU_LIB=yes], [HAVE_VPU_LIB=no])
if test "x$HAVE_VPU_LIB" = "xyes"; then
    VPU_LIBS=`$PKG_CONFIG --libs libfslvpuwrap 2>/dev/null`
fi
AC_SUBST(VPU_LIBS)

AC_CHECK_HEADERS([linux/ipu.h], [HAVE_IPU_HEAD=yes], [HAVE_IPU_HEAD=no])
if test "x$HAVE_IPU_HEAD" = "xno"; then
AC_CHECK_HEADERS([ipu.h], [HAVE_IPU_HEAD=yes], [HAVE_IPU_HEAD=no])
fi
if test "x$HAVE_IPU_HEAD" = "xno"; then
    ipuhead=`find $CROSS_ROOT/usr/src/ -name ipu.h | grep "/linux/ipu.h" | head -n 1 | sed -e 's,/linux/ipu.h,,'`
    if test "x$ipuhead" != "x"; then
        IPU_CFLAGS=-I$ipuhead
    fi
fi
AC_CHECK_LIB(ipu, mxc_ipu_lib_task_init, [IPU_LIBS=-lipu], [echo "No libipu around, don't use it"])
AC_SUBST(IPU_CFLAGS)
AC_SUBST(IPU_LIBS)

dnl of the POSIX Real-Time Extensions.
AC_CHECK_LIB(rt, clock_gettime)
AC_SUBST(RT_CFLAGS)
AC_SUBST(RT_LIBS)

FSL_MM_CORE_CFLAGS="`$PKG_CONFIG --cflags libfslaudiocodec 2>/dev/null` \
                    `$PKG_CONFIG --cflags libfslvideocodec 2>/dev/null` \
                    `$PKG_CONFIG --cflags imx-parser 2>/dev/null` \
                    `$PKG_CONFIG --cflags libfslvpuwrap 2>/dev/null`"
AC_SUBST(FSL_MM_CORE_CFLAGS)

FSL_MM_CORE_LIBS="`$PKG_CONFIG --libs libfslaudiocodec 2>/dev/null` \
                  `$PKG_CONFIG --libs libfslvideocodec 2>/dev/null` \
                  `$PKG_CONFIG --libs imx-parser 2>/dev/null`"
AC_SUBST(FSL_MM_CORE_LIBS)

EXTRAPLATFORMCFLAGS="-DARM -D_GNUC_"
XTRAINCPATHS="$FSL_MM_CORE_CFLAGS"
CFLAGS="$CFLAGS $XTRAINCPATHS $EXTRAPLATFORMCFLAGS"

FIND_PARSER_SONAME(asf,arm9)
FIND_PARSER_SONAME(asf,arm11)
FIND_PARSER_SONAME(asf,arm)
FIND_PARSER_SONAME(avi,arm9)
FIND_PARSER_SONAME(avi,arm11)
FIND_PARSER_SONAME(avi,arm)
FIND_PARSER_SONAME(flv,arm9)
FIND_PARSER_SONAME(flv,arm11)
FIND_PARSER_SONAME(flv,arm)
FIND_PARSER_SONAME(mpg2,arm9)
FIND_PARSER_SONAME(mpg2,arm11)
FIND_PARSER_SONAME(mpg2,arm)
FIND_PARSER_SONAME(mp4,arm9)
FIND_PARSER_SONAME(mp4,arm11)
FIND_PARSER_SONAME(mp4,arm)
FIND_PARSER_SONAME(mkv,arm9)
FIND_PARSER_SONAME(mkv,arm11)
FIND_PARSER_SONAME(mkv,arm)
FIND_PARSER_SONAME(ogg,arm9)
FIND_PARSER_SONAME(ogg,arm11)
FIND_PARSER_SONAME(ogg,arm)
FIND_PARSER_SONAME(rm,arm9)
FIND_PARSER_SONAME(rm,arm11)
FIND_PARSER_SONAME(rm,arm)
FIND_PARSER_SONAME(ape,arm9)
FIND_PARSER_SONAME(ape,arm11)
FIND_PARSER_SONAME(ape,arm)

FIND_ACODEC_SONAME(aacplusd, arm9)
FIND_ACODEC_SONAME(aacplusd, arm11)
FIND_ACODEC_SONAME(aacplusd, arm12)
FIND_ACODEC_SONAME(aacplusd, arm)
FIND_ACODEC_SONAME(aacd, arm9)
FIND_ACODEC_SONAME(aacd, arm11)
FIND_ACODEC_SONAME(aacd, arm12)
FIND_ACODEC_SONAME(aacd, arm)
FIND_ACODEC_SONAME(mp3d, arm9)
FIND_ACODEC_SONAME(mp3d, arm11)
FIND_ACODEC_SONAME(mp3d, arm12)
FIND_ACODEC_SONAME(mp3d, arm)
FIND_ACODEC_SONAME(wma10d, arm9)
FIND_ACODEC_SONAME(wma10d, arm11)
FIND_ACODEC_SONAME(wma10d, arm12)
FIND_ACODEC_SONAME(wma10d, arm)
FIND_ACODEC_SONAME(ac3d, arm11)
FIND_ACODEC_SONAME(ac3d, arm12)
FIND_ACODEC_SONAME(ac3d, arm)
FIND_ACODEC_SONAME(vorbisd, arm11)
FIND_ACODEC_SONAME(vorbisd, arm12)
FIND_ACODEC_SONAME(vorbisd, arm)
FIND_ACODEC_SONAME(ddpd, arm12)
FIND_ACODEC_SONAME(ddpd, arm)
FIND_ACODEC_SONAME(nbamrd, arm11)
FIND_ACODEC_SONAME(nbamrd, arm)
FIND_ACODEC_SONAME(wbamrd, arm12)
FIND_ACODEC_SONAME(wbamrd, arm)
FIND_ACODEC_SONAME(realad, arm9)
FIND_ACODEC_SONAME(realad, arm11)
FIND_ACODEC_SONAME(realad, arm)
FIND_ACODEC_SONAME(dsp, arm)

dnl set the plugindir where plugins should be installed
plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

AM_CONDITIONAL(PLATFORM_IS_MX6X, test "x$PLATFORM" = "xMX6" || test "x$PLATFORM" = "xMX6" || test "x$PLATFORM" = "xMX6SL" || test "x$PLATFORM" = "xMX6SX" || test "x$PLATFORM" = "xMX6UL" || test "x$PLATFORM" = "xMX6SLL" || test "x$PLATFORM" = "xMX6QP")
AM_CONDITIONAL(PLATFORM_IS_MX6, test "x$PLATFORM" = "xMX6")
AM_CONDITIONAL(PLATFORM_IS_MX6SL, test "x$PLATFORM" = "xMX6SL")
AM_CONDITIONAL(PLATFORM_IS_MX6SX, test "x$PLATFORM" = "xMX6SX")
AM_CONDITIONAL(PLATFORM_IS_MX6UL, test "x$PLATFORM" = "xMX6UL")
AM_CONDITIONAL(PLATFORM_IS_MX6SLL, test "x$PLATFORM" = "xMX6SLL")
AM_CONDITIONAL(PLATFORM_IS_MX6QP, test "x$PLATFORM" = "xMX6QP")
AM_CONDITIONAL(PLATFORM_IS_MX7D, test "x$PLATFORM" = "xMX7D")
AM_CONDITIONAL(PLATFORM_IS_MX7ULP, test "x$PLATFORM" = "xMX7ULP")
AM_CONDITIONAL(PLATFORM_IS_ARCH32, test "x$PLATFORM" = "xMX7D" || test "x$PLATFORM" = "xMX6" || test "x$PLATFORM" = "xMX6" || test "x$PLATFORM" = "xMX6SL" || test "x$PLATFORM" = "xMX6SX" || test "x$PLATFORM" = "xMX6SLL" || test "x$PLATFORM" = "xMX6UL" || test "x$PLATFORM" = "xMX6QP" || test "x$PLATFORM" = "xMX7ULP")
AM_CONDITIONAL(PLATFORM_IS_MX8, test "x$PLATFORM" = "xMX8")

AM_CONDITIONAL(USE_FB_DISPLAY, test "x$PLATFORM" = "xMX8" || test "x$PLATFORM" = "xMX7ULP")

if test "x$PLATFORM" = "xMX6" || test "x$PLATFORM" = "xMX6QP"; then
	 HAVE_DEVICE_IPU=yes
	 HAVE_DEVICE_G2D=yes
	 HAVE_DEVICE_PXP=yes
   #HAVE_DEVICE_PXP is enabled for consolidate rootfs when PLATFORM=MX6 for 6sx
fi

if test "x$PLATFORM" = "xMX6SL" || test "x$PLATFORM" = "xMX6SX"; then
	 HAVE_DEVICE_G2D=yes
	 HAVE_DEVICE_PXP=yes
fi

if test "x$PLATFORM" = "xMX6UL" || test "x$PLATFORM" = "xMX7D" || test "x$PLATFORM" = "xMX6SLL" ; then
	 HAVE_DEVICE_PXP=yes
fi

if test "x$PLATFORM" = "xMX8" || test "x$PLATFORM" = "xMX7ULP"; then
	 HAVE_DEVICE_G2D=yes
fi

CHECK_DISABLE_FEATURE(mp3enc, [Disable mp3 encoder plugin], [MP3_ENC], [mp3_enc_interface.h], [plugin: imxmp3enc])
CHECK_DISABLE_FEATURE(wma8enc, [Disable wma8 encoder plugin], [WMA8_ENC], [wma8_enc_interface.h], [plugin: wma8_enc])
CHECK_DISABLE_FEATURE(vpuwrap, [Disable vpu plugin], [VPU_WRAP], [vpu_wrapper.h], [plugin: vpu_wrap])
CHECK_DISABLE_FEATURE(aiur, [Disable aiur demux], [AIUR], [fsl_parser.h], [plugin: aiur])
CHECK_DISABLE_FEATURE(beep, [Disable beep audio decoder], [BEEP], [fsl_unia.h], [plugin: beep])
CHECK_DISABLE_FEATURE(v4lsink, [Disable fsl v4l sink], [V4L_SINK], [linux/videodev2.h], [plugin: v4lsink])
CHECK_DISABLE_FEATURE(overlaysink, [Disable fsl overlaysink], [OVERLAY_SINK], [g2d.h], [plugin: overlaysink])
CHECK_DISABLE_FEATURE(x11, [Disable x11 lib], [X11], [X11/Xlib.h], [library: libX11])
CHECK_DISABLE_FEATURE_EX(imx2ddevice_g2d, [Disable fsl imx2ddevice_g2d], [IMX_2DDEVICE_G2D], [g2d.h], [imx2ddevice: g2d], [$HAVE_DEVICE_G2D])
CHECK_DISABLE_FEATURE_EX(imx2ddevice_ipu, [Disable fsl imx2ddevice_ipu], [IMX_2DDEVICE_IPU], [ipu.h], [imx2ddevice: ipu], [$HAVE_DEVICE_IPU])
CHECK_DISABLE_FEATURE_EX(imx2ddevice_pxp, [Disable fsl imx2ddevice_pxp], [IMX_2DDEVICE_PXP], [pxp_lib.h], [imx2ddevice: pxp], [$HAVE_DEVICE_PXP])

CHECK_DISABLE_FEATURE(v4l2_core, [Disable lib v4l2_core], [V4L2_CORE], [linux/mxcfb.h uapi/mxcfb.h], [libs: v4l2core])

# Allow headers to be inside include/uapi and include/linux
#saved_CFLAGS="$CFLAGS"
#CFLAGS="$CFLAGS $IPU_CFLAGS"

#CHECK_DISABLE_FEATURE(isink, [Disable fsl ipu sink], [I_SINK], [linux/mxcfb.h uapi/mxcfb.h], [plugin: isink])
#CHECK_DISABLE_FEATURE(ipucsc, [Disable fsl ipu csc], [I_CSC], [linux/mxcfb.h uapi/mxcfb.h], [plugin: ipucsc])

#CFLAGS=$saved_CFLAGS

AC_SUBST(GSTMXLIBS_VERSION)
GSTMXLIBS_VERSION=0:1:0

AC_OUTPUT(
Makefile
gstreamer-fsl.pc
libs/Makefile
plugins/aiurdemux/Makefile
plugins/aiurdemux/aiur_registry_1.0.arm11.cf
plugins/aiurdemux/aiur_registry_1.0.arm.cf
plugins/beepdec/Makefile
plugins/beepdec/beep_registry_1.0.arm12.cf
plugins/beepdec/beep_registry_1.0.arm.cf
plugins/v4l2/Makefile
plugins/overlay_sink/Makefile
plugins/vpu/Makefile
plugins/mp3enc/Makefile
plugins/videoconvert/Makefile
plugins/compositor/Makefile
plugins/fbdevsink/Makefile
tools/Makefile
tools/gplay2/Makefile
tools/grecorder/Makefile)

echo -e "Configure result:"
echo -e "\tEnabled features:$enabled_feature"
echo -e "\tDisabled features:$disabled_feature"
