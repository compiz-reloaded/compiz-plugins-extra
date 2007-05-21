## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libtrailfocus_la_LDFLAGS = $(PFLAGS)
libtrailfocus_la_LIBADD = @COMPIZ_LIBS@
libtrailfocus_la_SOURCES = trailfocus_options.c trailfocus.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(datadir)"'          \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libtrailfocus.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
