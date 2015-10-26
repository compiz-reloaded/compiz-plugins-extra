## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = metadata include po src images

EXTRA_DIST =                    \
    VERSION                     \
    compiz-animationaddon.pc.in

DISTCLEANFILES =     \
    po/stamp-it \
    po/.intltool-merge-cache

if ANIMATIONADDON_PLUGIN
animationaddondata = compiz-animationaddon.pc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(animationaddondata)
