## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if CUBEADDON_PLUGIN
libcubeaddon_la_LDFLAGS = $(PFLAGS)
libcubeaddon_la_LIBADD = @COMPIZ_LIBS@ @COMPIZCUBE_LIBS@
nodist_libcubeaddon_la_SOURCES = cubeaddon_options.c cubeaddon_options.h
dist_libcubeaddon_la_SOURCES = cubeaddon.c
endif

BUILT_SOURCES = $(nodist_libcubeaddon_la_SOURCES)

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

cubeaddon_options.h: ../../metadata/cubeaddon.xml.in
		$(BCOP_BIN) --header $@ $<

cubeaddon_options.c: ../../metadata/cubeaddon.xml.in cubeaddon_options.h
		$(BCOP_BIN) --source $@ $<
