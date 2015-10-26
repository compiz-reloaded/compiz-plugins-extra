## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libcrashhandler_la_LDFLAGS = $(PFLAGS)
libcrashhandler_la_LIBADD = @COMPIZ_LIBS@
nodist_libcrashhandler_la_SOURCES = crashhandler_options.c crashhandler_options.h
dist_libcrashhandler_la_SOURCES = crashhandler.c

BUILT_SOURCES = $(nodist_libcrashhandler_la_SOURCES)

AM_CPPFLAGS =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libcrashhandler.la

CLEANFILES = *_options.c *_options.h

crashhandler_options.h: ../../metadata/crashhandler.xml.in
		$(BCOP_BIN) --header $@ $<

crashhandler_options.c: ../../metadata/crashhandler.xml.in crashhandler_options.h
		$(BCOP_BIN) --source $@ $<
