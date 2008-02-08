## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libshelf_la_LDFLAGS = $(PFLAGS)
libshelf_la_LIBADD = @COMPIZ_LIBS@
libshelf_la_SOURCES = shelf_options.c shelf_options.h shelf.c

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

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<
