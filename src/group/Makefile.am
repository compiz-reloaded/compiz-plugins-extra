PFLAGS=-module -avoid-version -no-undefined  

if GROUP_PLUGIN
libgroup_la_LDFLAGS = $(PFLAGS)
libgroup_la_LIBADD = @COMPIZ_LIBS@ @GROUP_LIBS@
libgroup_la_SOURCES = group_options.c \
		      group_options.h \
		      cairo.c         \
		      group.c         \
		      init.c          \
		      paint.c         \
		      queues.c        \
		      selection.c     \
		      tab.c           \
		      group-internal.h \
		      group_glow.h 
endif

BUILT_SOURCES = $(nodist_libgroup_la_SOURCES)

INCLUDES =                              \
	@COMPIZ_CFLAGS@                      \
 	@GROUP_CFLAGS@         \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

if GROUP_PLUGIN
module_LTLIBRARIES=libgroup.la
endif

CLEANFILES = *_options.c *_options.h

group_options.h: ../../metadata/group.xml.in
		$(BCOP_BIN) --header $@ $<

group_options.c: ../../metadata/group.xml.in group_options.h
		$(BCOP_BIN) --source $@ $<		
