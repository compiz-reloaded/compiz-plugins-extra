xmldir       = $(metadatadir)
xml_in_files = 	         \
	3d.xml.in \
	addhelper.xml.in \
	bench.xml.in \
	bicubic.xml.in \
	crashhandler.xml.in \
	cubecaps.xml.in \
	cubereflex.xml.in \
	extrawm.xml.in \
	fadedesktop.xml.in \
	firepaint.xml.in \
	gears.xml.in \
	group.xml.in \
	loginout.xml.in \
	maximumize.xml.in \
	mblur.xml.in \
	notification.xml.in \
	reflex.xml.in \
	scalefilter.xml.in \
	shelf.xml.in \
	showdesktop.xml.in \
	showmouse.xml.in \
	splash.xml.in \
	trailfocus.xml.in \
	widget.xml.in

xml_files    = $(xml_in_files:.xml.in=.xml)
xml_DATA     = $(xml_files)

@INTLTOOL_XML_RULE@

if USE_SCHEMAS
schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_files = $(patsubst %.xml.in,compiz-%.schemas,$(xml_in_files))
schema_DATA  = $(schema_files)

%.schemas: $(xml_files)
	xsltproc -o $@ $(xsltdir)/schemas.xslt $(subst compiz-,,$*).xml;

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
               GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(schema_DATA); \
       fi
endif
endif

EXTRA_DIST =		\
	$(xml_files)    \
	$(xml_in_files) \
	$(schema_files)

DISTCLEANFILES =	\
	$(xml_files)    \
	$(schema_files)

