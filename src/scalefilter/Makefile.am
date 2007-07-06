## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libscalefilter_la_LDFLAGS = $(PFLAGS)
libscalefilter_la_LIBADD = @COMPIZ_LIBS@
libscalefilter_la_SOURCES = scalefilter_options.c \
			    scalefilter_options.h \
			    scalefilter.c

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
