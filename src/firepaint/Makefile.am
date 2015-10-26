## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libfirepaint_la_LDFLAGS = $(PFLAGS)
libfirepaint_la_LIBADD = @COMPIZ_LIBS@
nodist_libfirepaint_la_SOURCES = firepaint_options.c firepaint_options.h
dist_libfirepaint_la_SOURCES = firepaint.c firepaint_tex.h

BUILT_SOURCES = $(nodist_libfirepaint_la_SOURCES)

AM_CPPFLAGS =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libfirepaint.la

CLEANFILES = *_options.c *_options.h

firepaint_options.h: ../../metadata/firepaint.xml.in
		$(BCOP_BIN) --header $@ $<

firepaint_options.c: ../../metadata/firepaint.xml.in firepaint_options.h
		$(BCOP_BIN) --source $@ $<
