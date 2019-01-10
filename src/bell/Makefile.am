## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if BELL_PLUGIN
libbell_la_LDFLAGS = $(PFLAGS)
libbell_la_LIBADD = $(COMPIZ_LIBS) $(BELL_LIBS)
nodist_libbell_la_SOURCES = bell_options.c bell_options.h
dist_libbell_la_SOURCES = bell.c

BUILT_SOURCES = $(nodist_libbell_la_SOURCES)

AM_CPPFLAGS =                               \
	-I$(top_srcdir)/include             \
	$(COMPIZ_CFLAGS)                    \
	$(BELL_CFLAGS)                      \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          

moduledir = $(plugindir)


module_LTLIBRARIES = libbell.la

CLEANFILES = bell_options.c bell_options.h

bell_options.h: $(top_srcdir)/metadata/bell.xml.in
	$(BCOP_BIN) --header $@ $<

bell_options.c: $(top_srcdir)/metadata/bell.xml.in bell_options.h
	$(BCOP_BIN) --source $@ $<
endif
