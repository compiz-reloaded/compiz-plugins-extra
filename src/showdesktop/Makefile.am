## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libshowdesktop_la_LDFLAGS = $(PFLAGS)
libshowdesktop_la_LIBADD = @COMPIZ_LIBS@
nodist_libshowdesktop_la_SOURCES = showdesktop_options.c showdesktop_options.h
dist_libshowdesktop_la_SOURCES = showdesktop.c

BUILT_SOURCES = $(nodist_libshowdesktop_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libshowdesktop.la

CLEANFILES = *_options.c *_options.h

showdesktop_options.h: ../../metadata/showdesktop.xml.in
		$(BCOP_BIN) --header $@ $<

showdesktop_options.c: ../../metadata/showdesktop.xml.in showdesktop_options.h
		$(BCOP_BIN) --source $@ $<
