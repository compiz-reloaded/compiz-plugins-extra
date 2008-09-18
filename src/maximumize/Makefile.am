## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libmaximumize_la_LDFLAGS = $(PFLAGS)
libmaximumize_la_LIBADD = @COMPIZ_LIBS@
libmaximumize_la_SOURCES = maximumize_options.c \
			   maximumize_options.h \
			   maximumize.c

BUILT_SOURCES = $(nodist_libmaximumize_la_SOURCES)

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

maximumize_options.h: ../../metadata/maximumize.xml.in
		$(BCOP_BIN) --header $@ $<

maximumize_options.c: ../../metadata/maximumize.xml.in maximumize_options.h
		$(BCOP_BIN) --source $@ $<
