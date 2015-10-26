PFLAGS=-module -avoid-version -no-undefined -lGLU

if ANIMATIONADDON_PLUGIN
libanimationaddon_la_LDFLAGS = $(PFLAGS)
libanimationaddon_la_LIBADD = @COMPIZ_LIBS@ @COMPIZANIMATION_LIBS@
libanimationaddon_la_SOURCES = airplane3d.c     \
			       animationaddon.c \
			       animationaddon.h \
			       beamup.c         \
			       burn.c           \
			       domino.c         \
			       explode3d.c      \
			       fold3d.c         \
			       glide3.c         \
			       leafspread.c     \
			       particle.c       \
			       polygon.c        \
			       skewer.c			\
				   animation_tex.h
endif

AM_CPPFLAGS =                                  \
	-I$(top_srcdir)/include	            \
	@COMPIZ_CFLAGS@                     \
	@COMPIZANIMATION_CFLAGS@            \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'

moduledir = $(plugindir)

if ANIMATIONADDON_PLUGIN
module_LTLIBRARIES=libanimationaddon.la
endif
