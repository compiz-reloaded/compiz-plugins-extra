## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libextrawm_la_LDFLAGS = $(PFLAGS)
libextrawm_la_LIBADD = @COMPIZ_LIBS@
libextrawm_la_SOURCES = extrawm_options.c extrawm_options.h extrawm.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(datadir)"'          \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libextrawm.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
