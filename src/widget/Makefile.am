## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libwidget_la_LDFLAGS = $(PFLAGS)
libwidget_la_LIBADD = @COMPIZ_LIBS@
nodist_libwidget_la_SOURCES = widget_options.c widget_options.h
dist_libwidget_la_SOURCES = widget.c

BUILT_SOURCES = $(nodist_libwidget_la_SOURCES)

AM_CPPFLAGS =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libwidget.la

CLEANFILES = *_options.c *_options.h

widget_options.h: ../../metadata/widget.xml.in
		$(BCOP_BIN) --header $@ $<

widget_options.c: ../../metadata/widget.xml.in widget_options.h
		$(BCOP_BIN) --source $@ $<
