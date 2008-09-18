## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libfadedesktop_la_LDFLAGS = $(PFLAGS)
libfadedesktop_la_LIBADD = @COMPIZ_LIBS@
nodist_libfadedesktop_la_SOURCES = fadedesktop_options.c fadedesktop_options.h
dist_libfadedesktop_la_SOURCES = fadedesktop.c

BUILT_SOURCES = $(nodist_libfadedesktop_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libfadedesktop.la

CLEANFILES = *_options.c *_options.h

fadedesktop_options.h: ../../metadata/fadedesktop.xml.in
		$(BCOP_BIN) --header $@ $<

fadedesktop_options.c: ../../metadata/fadedesktop.xml.in fadedesktop_options.h
		$(BCOP_BIN) --source $@ $<
