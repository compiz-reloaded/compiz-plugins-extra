## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libaddhelper_la_LDFLAGS = $(PFLAGS)
libaddhelper_la_LIBADD = @COMPIZ_LIBS@
nodist_libaddhelper_la_SOURCES = addhelper_options.c addhelper_options.h
dist_libaddhelper_la_SOURCES = addhelper.c

BUILT_SOURCES = $(nodist_libaddhelper_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libaddhelper.la

CLEANFILES = *_options.c *_options.h

addhelper_options.h: ../../metadata/addhelper.xml.in
		$(BCOP_BIN) --header $@ $<

addhelper_options.c: ../../metadata/addhelper.xml.in addhelper_options.h
		$(BCOP_BIN) --source $@ $<
