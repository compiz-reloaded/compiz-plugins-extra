## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libreflex_la_LDFLAGS = $(PFLAGS)
libreflex_la_LIBADD = @COMPIZ_LIBS@
nodist_libreflex_la_SOURCES = reflex_options.c reflex_options.h
dist_libreflex_la_SOURCES = reflex.c

BUILT_SOURCES = $(nodist_libreflex_la_SOURCES)

AM_CPPFLAGS =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libreflex.la

CLEANFILES = *_options.c *_options.h

reflex_options.h: ../../metadata/reflex.xml.in
		$(BCOP_BIN) --header $@ $<

reflex_options.c: ../../metadata/reflex.xml.in reflex_options.h
		$(BCOP_BIN) --source $@ $<
