AC_PREREQ(2.57)

AC_INIT([compiz-fusion-plugins-extra],esyscmd(. ./VERSION;echo -n $VERSION), [maniac@opencompositing.org])

#AC_CONFIG_AUX_DIR(config)

AM_INIT_AUTOMAKE([1.9 dist-bzip2])
AC_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_LIBTOOL
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h sys/time.h unistd.h])
IT_PROG_INTLTOOL([0.35.0])
AC_SUBST(ALL_LINGUAS)
AM_GLIB_GNU_GETTEXT
GETTEXT_PACKAGE=compiz-fusion-plugins-extra
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
    PKG_CONFIG_PATH=${prefix}/lib/pkgconfig
        else
    PKG_CONFIG_PATH=${prefix}/lib/pkgconfig:${PKG_CONFIG_PATH}
fi
export PKG_CONFIG_PATH
AC_SUBST(PKG_CONFIG_PATH)
AC_MSG_NOTICE([Using PKG_CONFIG_PATH=$PKG_CONFIG_PATH])

PKG_CHECK_MODULES(COMPIZ, compiz)
PKG_CHECK_MODULES(BCOP, bcop >= 0.5.2, [bcop_found=yes])
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
if test "$use_group" = yes; then
  AC_DEFINE(USE_GROUP, 1, [Build group plugin])
fi

PKG_CHECK_MODULES(COMPIZCUBE, compiz-cube, [have_compiz_cube=yes], [have_compiz_cube=no])
AM_CONDITIONAL(CUBEREFLEX_PLUGIN, test "x$have_compiz_cube" = "xyes")
AM_CONDITIONAL(CUBECAPS_PLUGIN, test "x$have_compiz_cube" = "xyes")

PKG_CHECK_MODULES(COMPIZMOUSEPOLL, compiz-mousepoll, [have_compiz_mousepoll=yes], [have_compiz_mousepoll=no])
AM_CONDITIONAL(SHOWMOUSE_PLUGIN, test "x$have_compiz_mousepoll" = "xyes")


AC_ARG_ENABLE(schemas,
  [  --enable-schemas         Build gconf schemas],
  [use_schemas=$enableval], [use_schemas=no])

if test x"$use_schemas" = x"yes"; then
  AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
  PKG_CHECK_MODULES(SCHEMAS, compiz-gconf, [use_schemas=yes], [use_schemas=no])
  if test x"$GCONFTOOL" = xno; then
    AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
  fi
  AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
  if test x"$PKG_CONFIG" = xno ; then
    AC_MSG_ERROR([pkg-config executable not found in your path, unable to determine schema directory])
  fi
  xsltdir=`$PKG_CONFIG --variable=xsltdir compiz-gconf`
  AC_SUBST(xsltdir)
fi

AM_GCONF_SOURCE_2


AM_CONDITIONAL(USE_SCHEMAS, test "x$use_schemas" = "xyes")
if test "$use_schemas" = yes; then
  AC_DEFINE(USE_SCHEMAS, 1, [Build gconf schemas])
fi

AC_OUTPUT([
Makefile
images/Makefile
metadata/Makefile
src/Makefile
src/addhelper/Makefile
src/bench/Makefile
src/crashhandler/Makefile
src/cubecaps/Makefile
src/cubereflex/Makefile
src/extrawm/Makefile
src/fadedesktop/Makefile
src/firepaint/Makefile
src/gears/Makefile
src/group/Makefile
src/mblur/Makefile
src/reflex/Makefile
src/scalefilter/Makefile
src/showdesktop/Makefile
src/splash/Makefile
src/trailfocus/Makefile
src/widget/Makefile
src/showmouse/Makefile
src/loginout/Makefile
po/Makefile.in
])
