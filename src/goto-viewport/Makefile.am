## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libgotovp_la_LDFLAGS = $(PFLAGS)
libgotovp_la_LIBADD = @COMPIZ_LIBS@
libgotovp_la_SOURCES = gotovp_options.c gotovp_options.h gotovp.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libgotovp.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
