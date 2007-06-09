## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libcrashhandler_la_LDFLAGS = $(PFLAGS)
libcrashhandler_la_LIBADD = @COMPIZ_LIBS@
libcrashhandler_la_SOURCES = crashhandler_options.c crashhandler_options.h crashhandler.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(datadir)"'          \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libcrashhandler.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
