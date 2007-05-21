## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libfirepaint_la_LDFLAGS = $(PFLAGS)
libfirepaint_la_LIBADD = @COMPIZ_LIBS@
libfirepaint_la_SOURCES = firepaint_options.c firepaint.c firepaint_tex.h

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(datadir)"'          \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libfirepaint.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
