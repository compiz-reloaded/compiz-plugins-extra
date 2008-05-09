## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libwallpaper_la_LDFLAGS = $(PFLAGS)
libwallpaper_la_LIBADD = @COMPIZ_LIBS@
libwallpaper_la_SOURCES = wallpaper_options.c wallpaper_options.h wallpaper.c

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libwallpaper.la

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
