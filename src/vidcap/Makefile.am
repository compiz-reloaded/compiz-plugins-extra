## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libvidcap_la_LDFLAGS = $(PFLAGS)
libvidcap_la_LIBADD = @COMPIZ_LIBS@
nodist_libvidcap_la_SOURCES = vidcap_options.c vidcap_options.h
dist_libvidcap_la_SOURCES = vidcap.c wcap-decode.h

BUILT_SOURCES = $(nodist_libvidcap_la_SOURCES)

AM_CPPFLAGS =                               \
	@COMPIZ_CFLAGS@                     \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libvidcap.la

CLEANFILES = *_options.c *_options.h

vidcap_options.h: ../../metadata/vidcap.xml.in
		$(BCOP_BIN) --header $@ $<

vidcap_options.c: ../../metadata/vidcap.xml.in vidcap_options.h
		$(BCOP_BIN) --source $@ $<
