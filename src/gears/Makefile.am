## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libgears_la_LDFLAGS = $(PFLAGS)
libgears_la_LIBADD = @COMPIZ_LIBS@
nodist_libgears_la_SOURCES = gears_options.c gears_options.h
dist_libgears_la_SOURCES = gears.c

BUILT_SOURCES = $(nodist_libgears_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libgears.la

CLEANFILES = *_options.c *_options.h

gears_options.h: ../../metadata/gears.xml.in
		$(BCOP_BIN) --header $@ $<

gears_options.c: ../../metadata/gears.xml.in gears_options.h
		$(BCOP_BIN) --source $@ $<
