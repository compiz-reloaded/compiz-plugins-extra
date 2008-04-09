## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if CUBEADDON_PLUGIN
libcubeaddon_la_LDFLAGS = $(PFLAGS)
libcubeaddon_la_LIBADD = @COMPIZ_LIBS@ @COMPIZCUBE_LIBS@
libcubeaddon_la_SOURCES = cubeaddon_options.c cubeaddon_options.h cubeaddon.c
endif

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	@COMPIZCUBE_CFLAGS@              \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

if CUBEADDON_PLUGIN
module_LTLIBRARIES = libcubeaddon.la
endif

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
