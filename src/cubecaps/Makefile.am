## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if CUBECAPS_PLUGIN
libcubecaps_la_LDFLAGS = $(PFLAGS)
libcubecaps_la_LIBADD = @COMPIZ_LIBS@ @COMPIZCUBE_LIBS@
libcubecaps_la_SOURCES = cubecaps_options.c cubecaps_options.h cubecaps.c
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

if CUBECAPS_PLUGIN
module_LTLIBRARIES = libcubecaps.la
endif

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
