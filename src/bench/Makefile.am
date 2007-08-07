## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libbench_la_LDFLAGS = $(PFLAGS)
libbench_la_LIBADD = @COMPIZ_LIBS@
libbench_la_SOURCES = bench_options.c bench_options.h bench.c bench_tex.h

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

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
