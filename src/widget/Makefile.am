## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libwidget_la_LDFLAGS = $(PFLAGS)
libwidget_la_LIBADD = @COMPIZ_LIBS@
libwidget_la_SOURCES = widget_options.c widget_options.h widget.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libwidget.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
