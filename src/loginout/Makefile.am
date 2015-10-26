## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libloginout_la_LDFLAGS = $(PFLAGS)
libloginout_la_LIBADD = @COMPIZ_LIBS@
nodist_libloginout_la_SOURCES = loginout_options.c loginout_options.h
dist_libloginout_la_SOURCES = loginout.c

BUILT_SOURCES = $(nodist_libloginout_la_SOURCES)

AM_CPPFLAGS =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libloginout.la

CLEANFILES = *_options.c *_options.h

loginout_options.h: ../../metadata/loginout.xml.in
		$(BCOP_BIN) --header $@ $<

loginout_options.c: ../../metadata/loginout.xml.in loginout_options.h
		$(BCOP_BIN) --source $@ $<
