## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if HIGHLIGHTCONTENT_PLUGIN
libhighlightcontent_la_LDFLAGS = $(PFLAGS)
libhighlightcontent_la_LIBADD = @COMPIZ_LIBS@ @HIGHLIGHTCONTENT_LIBS@
nodist_libhighlightcontent_la_SOURCES = highlightcontent_options.c highlightcontent_options.h
dist_libhighlightcontent_la_SOURCES = highlightcontent.c
endif

BUILT_SOURCES = $(nodist_libhighlightcontent_la_SOURCES)

AM_CPPFLAGS =                              \
	@COMPIZ_CFLAGS@                  \
	@HIGHLIGHTCONTENT_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

if HIGHLIGHTCONTENT_PLUGIN
module_LTLIBRARIES = libhighlightcontent.la
endif

CLEANFILES = *_options.c *_options.h

highlightcontent_options.h: ../../metadata/highlightcontent.xml.in
		$(BCOP_BIN) --header $@ $<

highlightcontent_options.c: ../../metadata/highlightcontent.xml.in highlightcontent_options.h
		$(BCOP_BIN) --source $@ $<
