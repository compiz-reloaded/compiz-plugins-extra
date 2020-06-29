ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = metadata include po src images icons

EXTRA_DIST =			    \
	AUTHORS			    \
	COPYING			    \
	NEWS			    \
	README.md		    \
	autogen.sh		    \
	compiz-animationaddon.pc.in \
	gettext

if ANIMATIONADDON_PLUGIN
animationaddondata = compiz-animationaddon.pc
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(animationaddondata)

# Build ChangeLog from GIT history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
