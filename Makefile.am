ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = metadata include po src images

EXTRA_DIST =			   \
	AUTHORS			    \
	COPYING			    \
	NEWS			    \
	README.md		    \
	autogen.sh		    \
	compiz-animationaddon.pc.in \
	intltool-extract.in	    \
	intltool-merge.in	    \
	intltool-update.in

DISTCLEANFILES =		\
	po/stamp-it		 \
	intltool-extract	 \
	intltool-merge		 \
	intltool-update		 \
	po/.intltool-merge-cache

if ANIMATIONADDON_PLUGIN
animationaddondata = compiz-animationaddon.pc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(animationaddondata)
