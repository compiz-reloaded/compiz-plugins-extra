## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libaddhelper_la_LDFLAGS = $(PFLAGS)
libaddhelper_la_LIBADD = @COMPIZ_LIBS@
libaddhelper_la_SOURCES = addhelper_options.c addhelper_options.h addhelper.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libaddhelper.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
