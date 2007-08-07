## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libmblur_la_LDFLAGS = $(PFLAGS)
libmblur_la_LIBADD = @COMPIZ_LIBS@
libmblur_la_SOURCES = mblur_options.c mblur_options.h mblur.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libmblur.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
