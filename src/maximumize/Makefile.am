## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libmaximumize_la_LDFLAGS = $(PFLAGS)
libmaximumize_la_LIBADD = @COMPIZ_LIBS@
libmaximumize_la_SOURCES = maximumize_options.c \
			   maximumize_options.h \
			   maximumize.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libmaximumize.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
