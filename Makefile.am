## Process this file with automake to produce Makefile.in
SUBDIRS = metadata include po src images

EXTRA_DIST =                    \
    VERSION                     \
    compiz-animationaddon.pc.in \
    intltool-extract.in         \
    intltool-merge.in           \
    intltool-update.in          \
    intltool-extract            \
    intltool-merge              \
    intltool-update 

DISTCLEANFILES =     \
    intltool-extract \
    intltool-merge   \
    intltool-update  \
    po/stamp-it \
    po/.intltool-merge-cache

if ANIMATIONADDON_PLUGIN
animationaddondata = compiz-animationaddon.pc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(animationaddondata)
