## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

if THREED_PLUGIN
lib3d_la_LDFLAGS = $(PFLAGS)
lib3d_la_LIBADD = @COMPIZ_LIBS@ @COMPIZCUBE_LIBS@
nodist_lib3d_la_SOURCES = 3d_options.c 3d_options.h
dist_lib3d_la_SOURCES = 3d.c
endif

BUILT_SOURCES = $(nodist_lib3d_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	@COMPIZCUBE_CFLAGS@              \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

if THREED_PLUGIN
module_LTLIBRARIES = lib3d.la
endif

CLEANFILES = *_options.c *_options.h

3d_options.h: ../../metadata/3d.xml.in
		$(BCOP_BIN) --header $@ $<

3d_options.c: ../../metadata/3d.xml.in 3d_options.h
		$(BCOP_BIN) --source $@ $<
