PFLAGS=-module -avoid-version -no-undefined  

if GROUP_PLUGIN
libgroup_la_LDFLAGS = $(PFLAGS)
libgroup_la_LIBADD = @COMPIZ_LIBS@ @GROUP_LIBS@
libgroup_la_SOURCES = group_options.c group.c init.c paint.c queues.c selection.c tab.c group.h group_glow.h 
endif

INCLUDES =                              \
	@COMPIZ_CFLAGS@                      \
 	@GROUP_CFLAGS@         \
	-DDATADIR='"$(datadir)"'            \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

if GROUP_PLUGIN
module_LTLIBRARIES=libgroup.la
endif

CLEANFILES = *_options.c *_options.h

%_options.h: ../../metadata/%.xml
		$(BCOP_BIN) --header $@ $<

%_options.c: ../../metadata/%.xml %_options.h
		$(BCOP_BIN) --source $@ $<		
