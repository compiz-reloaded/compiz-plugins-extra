## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libgears_la_LDFLAGS = $(PFLAGS)
libgears_la_LIBADD = @COMPIZ_LIBS@
libgears_la_SOURCES = gears_options.c gears_options.h gears.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(datadir)"'          \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libgears.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
