## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if SHOWMOUSE_PLUGIN
libshowmouse_la_LDFLAGS = $(PFLAGS)
libshowmouse_la_LIBADD = @COMPIZ_LIBS@ @SHOWMOUSE_LIBS@
nodist_libshowmouse_la_SOURCES = showmouse_options.c showmouse_options.h
dist_libshowmouse_la_SOURCES = showmouse.c showmouse_tex.h
endif

BUILT_SOURCES = $(nodist_libshowmouse_la_SOURCES)

AM_CPPFLAGS =                              \
	@COMPIZ_CFLAGS@                  \
	@SHOWMOUSE_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

if SHOWMOUSE_PLUGIN
module_LTLIBRARIES = libshowmouse.la
endif

CLEANFILES = *_options.c *_options.h

showmouse_options.h: ../../metadata/showmouse.xml.in
		$(BCOP_BIN) --header $@ $<

showmouse_options.c: ../../metadata/showmouse.xml.in showmouse_options.h
		$(BCOP_BIN) --source $@ $<
