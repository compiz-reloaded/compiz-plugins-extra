AC_PREREQ(2.57)

AC_INIT([compiz-plugins-extra], [0.8.9], [maniac@opencompositing.org])
AM_INIT_AUTOMAKE([1.9 foreign dist-xz no-dist-gzip])
AC_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h sys/time.h unistd.h])
IT_PROG_INTLTOOL([0.35.0])
AC_CONFIG_MACRO_DIR([m4])
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT
GETTEXT_PACKAGE=compiz-plugins-extra
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])
AC_SUBST(GETTEXT_PACKAGE)

if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[[\ \	]]-Wall[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wpointer-arith[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wpointer-arith" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wstrict-prototypes[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wstrict-prototypes" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wmissing-prototypes[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wmissing-prototypes" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wmissing-declarations[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wmissing-declarations" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-Wnested-externs[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -Wnested-externs" ;;
  esac

  case " $CFLAGS " in
  *[[\ \	]]-fno-strict-aliasing[[\ \	]]*) ;;
  *) CFLAGS="$CFLAGS -fno-strict-aliasing" ;;
  esac

  if test "x$enable_ansi" = "xyes"; then
    case " $CFLAGS " in
    *[[\ \	]]-ansi[[\ \	]]*) ;;
    *) CFLAGS="$CFLAGS -ansi" ;;
    esac

    case " $CFLAGS " in
    *[[\ \	]]-pedantic[[\ \	]]*) ;;
    *) CFLAGS="$CFLAGS -pedantic" ;;
    esac
  fi
fi

AC_C_BIGENDIAN

plugindir=$libdir/compiz
AC_SUBST(plugindir)

imagedir=$datadir/compiz
AC_SUBST(imagedir)

metadatadir=$datadir/compiz
AC_SUBST(metadatadir)


dnl ============================================================
dnl Check for the pkg-config path.
if test x"$PKG_CONFIG_PATH" = x; then
    PKG_CONFIG_PATH=${prefix}/lib/pkgconfig:${prefix}/share/pkgconfig
        else
    PKG_CONFIG_PATH=${prefix}/lib/pkgconfig:${prefix}/share/pkgconfig:${PKG_CONFIG_PATH}
fi
export PKG_CONFIG_PATH
AC_SUBST(PKG_CONFIG_PATH)
AC_MSG_NOTICE([Using PKG_CONFIG_PATH=$PKG_CONFIG_PATH])

PKG_CHECK_MODULES(COMPIZ, compiz)
PKG_CHECK_MODULES(BCOP, bcop >= 0.7.3, [bcop_found=yes])
if test "$bcop_found" = yes; then
  if test -z "$PKG_CONFIG"; then
      AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  fi  
  if test "$PKG_CONFIG" != "no" ; then
      BCOP_BIN=`$PKG_CONFIG --variable=bin bcop`
      AC_SUBST(BCOP_BIN)
  fi
fi

AC_MSG_CHECKING(for GL_CFLAGS)
AC_ARG_WITH(gl-cflags, [  --with-gl-cflags=CFLAGS ],
		       [GL_CFLAGS="$withval"],
		       [GL_CFLAGS=""])

AC_MSG_RESULT($GL_CFLAGS)
AC_MSG_CHECKING(for GL_LIBS)
AC_ARG_WITH(gl-libs, [  --with-gl-libs=LIBS ],
		     [GL_LIBS="$withval"],
		     [GL_LIBS="-lGL"])
AC_MSG_RESULT($GL_LIBS)

AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)


PKG_CHECK_MODULES(GROUP, compiz-text cairo >= 1.0, [use_group=yes], [use_group=no])
AM_CONDITIONAL(GROUP_PLUGIN, test "x$use_group" = "xyes")

PKG_CHECK_MODULES(SCALEFILTER, compiz-text compiz-scale, [use_scalefilter=yes], [use_scalefilter=no])
AM_CONDITIONAL(SCALEFILTER_PLUGIN, test "x$use_scalefilter" = "xyes")

PKG_CHECK_MODULES(COMPIZCUBE, compiz-cube, [have_compiz_cube=yes], [have_compiz_cube=no])
AM_CONDITIONAL(CUBEADDON_PLUGIN, test "x$have_compiz_cube" = "xyes")
AM_CONDITIONAL(THREED_PLUGIN, test "x$have_compiz_cube" = "xyes")

PKG_CHECK_MODULES(COMPIZMOUSEPOLL, compiz-mousepoll, [have_compiz_mousepoll=yes], [have_compiz_mousepoll=no])
AM_CONDITIONAL(SHOWMOUSE_PLUGIN, test "x$have_compiz_mousepoll" = "xyes")

PKG_CHECK_MODULES(LIBNOTIFY, libnotify gtk+-2.0, [have_libnotify=yes], [have_libnotify=no])
AM_CONDITIONAL(NOTIFICATION_PLUGIN, test "x$have_libnotify" = "xyes")
if test "$have_libnotify" = yes; then
  AC_DEFINE(USE_NOTIFICATION, 1, [Build notification plugin])
  PKG_CHECK_EXISTS(libnotify >= 0.6.1, [have_libnotify_0_6_1=yes], [have_libnotify_0_6_1=no])
  if test "$have_libnotify_0_6_1" = yes; then
    AC_DEFINE(HAVE_LIBNOTIFY_0_6_1, 1, [libnotify >= 0.6.1 present])
  fi
fi

PKG_CHECK_MODULES(COMPIZANIMATION, compiz-animation, [have_compiz_animation=yes], [have_compiz_animation=no])
AM_CONDITIONAL(ANIMATIONADDON_PLUGIN, test "x$have_compiz_animation" = "xyes")

AC_OUTPUT([
compiz-animationaddon.pc
Makefile
images/Makefile
include/Makefile
metadata/Makefile
src/Makefile
src/3d/Makefile
src/addhelper/Makefile
src/animationaddon/Makefile
src/bench/Makefile
src/bicubic/Makefile
src/crashhandler/Makefile
src/cubeaddon/Makefile
src/extrawm/Makefile
src/fadedesktop/Makefile
src/firepaint/Makefile
src/gears/Makefile
src/grid/Makefile
src/group/Makefile
src/maximumize/Makefile
src/mblur/Makefile
src/notification/Makefile
src/reflex/Makefile
src/scalefilter/Makefile
src/shelf/Makefile
src/showdesktop/Makefile
src/splash/Makefile
src/trailfocus/Makefile
src/widget/Makefile
src/showmouse/Makefile
src/loginout/Makefile
src/wallpaper/Makefile
po/Makefile.in
])
