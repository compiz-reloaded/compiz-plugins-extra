## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libreflex_la_LDFLAGS = $(PFLAGS)
libreflex_la_LIBADD = @COMPIZ_LIBS@
libreflex_la_SOURCES = reflex_options.c reflex_options.h reflex.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libreflex.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
