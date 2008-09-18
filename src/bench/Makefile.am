## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libbench_la_LDFLAGS = $(PFLAGS)
libbench_la_LIBADD = @COMPIZ_LIBS@
nodist_libbench_la_SOURCES = bench_options.c bench_options.h
dist_libbench_la_SOURCES = bench.c bench_tex.h

BUILT_SOURCES = $(nodist_libbench_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libbench.la

CLEANFILES = *_options.c *_options.h

bench_options.h: ../../metadata/bench.xml.in
		$(BCOP_BIN) --header $@ $<

bench_options.c: ../../metadata/bench.xml.in bench_options.h
		$(BCOP_BIN) --source $@ $<
