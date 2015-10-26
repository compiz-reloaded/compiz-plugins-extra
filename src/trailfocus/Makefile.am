## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libtrailfocus_la_LDFLAGS = $(PFLAGS)
libtrailfocus_la_LIBADD = @COMPIZ_LIBS@
nodist_libtrailfocus_la_SOURCES = trailfocus_options.c trailfocus_options.h
dist_libtrailfocus_la_SOURCES = trailfocus.c

BUILT_SOURCES = $(nodist_libtrailfocus_la_SOURCES)

AM_CPPFLAGS =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libtrailfocus.la

CLEANFILES = *_options.c *_options.h

trailfocus_options.h: ../../metadata/trailfocus.xml.in
		$(BCOP_BIN) --header $@ $<

trailfocus_options.c: ../../metadata/trailfocus.xml.in trailfocus_options.h
		$(BCOP_BIN) --source $@ $<
