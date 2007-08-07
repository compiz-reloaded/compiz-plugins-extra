## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libsplash_la_LDFLAGS = $(PFLAGS)
libsplash_la_LIBADD = @COMPIZ_LIBS@
libsplash_la_SOURCES = splash_options.c splash_options.h splash.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libsplash.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
