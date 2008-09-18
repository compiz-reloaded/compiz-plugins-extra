## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libextrawm_la_LDFLAGS = $(PFLAGS)
libextrawm_la_LIBADD = @COMPIZ_LIBS@
nodist_libextrawm_la_SOURCES = extrawm_options.c extrawm_options.h
dist_libextrawm_la_SOURCES = extrawm.c

BUILT_SOURCES = $(nodist_libextrawm_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libextrawm.la

CLEANFILES = *_options.c *_options.h

extrawm_options.h: ../../metadata/extrawm.xml.in
		$(BCOP_BIN) --header $@ $<

extrawm_options.c: ../../metadata/extrawm.xml.in extrawm_options.h
		$(BCOP_BIN) --source $@ $<
