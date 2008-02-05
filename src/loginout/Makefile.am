## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libloginout_la_LDFLAGS = $(PFLAGS)
libloginout_la_LIBADD = @COMPIZ_LIBS@
libloginout_la_SOURCES = loginout_options.c loginout_options.h loginout.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libloginout.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
