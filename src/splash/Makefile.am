## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libsplash_la_LDFLAGS = $(PFLAGS)
libsplash_la_LIBADD = @COMPIZ_LIBS@
nodist_libsplash_la_SOURCES = splash_options.c splash_options.h
dist_libsplash_la_SOURCES = splash.c

BUILT_SOURCES = $(nodist_libsplash_la_SOURCES)

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

splash_options.h: ../../metadata/splash.xml.in
		$(BCOP_BIN) --header $@ $<

splash_options.c: ../../metadata/splash.xml.in splash_options.h
		$(BCOP_BIN) --source $@ $<
