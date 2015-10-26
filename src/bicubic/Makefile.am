## Process this file with automake to produce Makefile.in
PFLAGS=-module -avoid-version -no-undefined

libbicubic_la_LDFLAGS = $(PFLAGS)
libbicubic_la_LIBADD = @COMPIZ_LIBS@
libbicubic_la_SOURCES = bicubic.c

AM_CPPFLAGS =                              \
	@COMPIZ_CFLAGS@                  \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

module_LTLIBRARIES = libbicubic.la
