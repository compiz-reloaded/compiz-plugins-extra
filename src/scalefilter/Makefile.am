## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if SCALEFILTER_PLUGIN
libscalefilter_la_LDFLAGS = $(PFLAGS)
libscalefilter_la_LIBADD = @COMPIZ_LIBS@ @SCALEFILTER_LIBS@
libscalefilter_la_SOURCES = scalefilter_options.c \
			    scalefilter_options.h \
			    scalefilter.c
endif

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	@SCALEFILTER_CFLAGS@                \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

if SCALEFILTER_PLUGIN
module_LTLIBRARIES = libscalefilter.la
endif

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
