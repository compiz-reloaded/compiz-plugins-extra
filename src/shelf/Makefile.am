## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libshelf_la_LDFLAGS = $(PFLAGS)
libshelf_la_LIBADD = @COMPIZ_LIBS@
nodist_libshelf_la_SOURCES = shelf_options.c shelf_options.h
dist_libshelf_la_SOURCES = shelf.c

BUILT_SOURCES = $(nodist_libshelf_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)


module_LTLIBRARIES = libshelf.la

CLEANFILES = *_options.c *_options.h

shelf_options.h: ../../metadata/shelf.xml.in
		$(BCOP_BIN) --header $@ $<

shelf_options.c: ../../metadata/shelf.xml.in shelf_options.h
		$(BCOP_BIN) --source $@ $<
