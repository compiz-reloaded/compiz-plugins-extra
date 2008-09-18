## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if SCALEFILTER_PLUGIN
libscalefilter_la_LDFLAGS = $(PFLAGS)
libscalefilter_la_LIBADD = @COMPIZ_LIBS@ @SCALEFILTER_LIBS@
libscalefilter_la_SOURCES = scalefilter_options.c \
			    scalefilter_options.h \
			    scalefilter.c
endif

BUILT_SOURCES = $(nodist_libscalefilter_la_SOURCES)

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

scalefilter_options.h: ../../metadata/scalefilter.xml.in
		$(BCOP_BIN) --header $@ $<

scalefilter_options.c: ../../metadata/scalefilter.xml.in scalefilter_options.h
		$(BCOP_BIN) --source $@ $<
