## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libgrid_la_LDFLAGS = $(PFLAGS)
libgrid_la_LIBADD = @COMPIZ_LIBS@
nodist_libgrid_la_SOURCES = grid_options.c grid_options.h
dist_libgrid_la_SOURCES = grid.c

BUILT_SOURCES = $(nodist_libgrid_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libgrid.la

CLEANFILES = *_options.c *_options.h

grid_options.h: ../../metadata/grid.xml.in
		$(BCOP_BIN) --header $@ $<

grid_options.c: ../../metadata/grid.xml.in grid_options.h
		$(BCOP_BIN) --source $@ $<
