## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if CUBEREFLEX_PLUGIN
libcubereflex_la_LDFLAGS = $(PFLAGS)
libcubereflex_la_LIBADD = @COMPIZ_LIBS@ @COMPIZCUBE_LIBS@
libcubereflex_la_SOURCES = cubereflex_options.c cubereflex_options.h cubereflex.c
endif

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	@COMPIZCUBE_CFLAGS@              \
	-DDATADIR='"$(datadir)"'          \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

if CUBEREFLEX_PLUGIN
module_LTLIBRARIES = libcubereflex.la
endif

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
