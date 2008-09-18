## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libwallpaper_la_LDFLAGS = $(PFLAGS)
libwallpaper_la_LIBADD = @COMPIZ_LIBS@
nodist_libwallpaper_la_SOURCES = wallpaper_options.c wallpaper_options.h
dist_libwallpaper_la_SOURCES = wallpaper.c

BUILT_SOURCES = $(nodist_libwallpaper_la_SOURCES)

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

wallpaper_options.h: ../../metadata/wallpaper.xml.in
		$(BCOP_BIN) --header $@ $<

wallpaper_options.c: ../../metadata/wallpaper.xml.in wallpaper_options.h
		$(BCOP_BIN) --source $@ $<
