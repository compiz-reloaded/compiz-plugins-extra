PFLAGS=-module -avoid-version -no-undefined  

if NOTIFICATION_PLUGIN
libnotification_la_LDFLAGS = $(PFLAGS)
libnotification_la_LIBADD = @COMPIZ_LIBS@ @LIBNOTIFY_LIBS@
libnotification_la_SOURCES = notification.c
endif

INCLUDES =                              \
	@COMPIZ_CFLAGS@                      \
 	@LIBNOTIFY_CFLAGS@         \
	-DDATADIR='"$(compdatadir)"'        \
	-DLIBDIR='"$(libdir)"'              \
	-DLOCALEDIR="\"@datadir@/locale\""  \
	-DIMAGEDIR='"$(imagedir)"'          \
	-I$(top_srcdir)/include

moduledir = $(plugindir)

if NOTIFICATION_PLUGIN
module_LTLIBRARIES=libnotification.la
endif
