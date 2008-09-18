xmldir       = $(metadatadir)

if ANIMATIONADDON_PLUGIN
animationaddonxml = animationaddon.xml.in
endif

if CUBEADDON_PLUGIN
cubeaddonxml = cubeaddon.xml.in
endif

if GROUP_PLUGIN
groupxml = group.xml.in
endif

if NOTIFICATION_PLUGIN
notificationxml = notification.xml.in
else
endif

if SCALEFILTER_PLUGIN
scalefilterxml = scalefilter.xml.in
endif

if SHOWMOUSE_PLUGIN
showmousexml = showmouse.xml.in
endif

if THREED_PLUGIN
threedxml = 3d.xml.in
endif

xml_in_files = 	             \
	addhelper.xml.in     \
	bench.xml.in         \
	bicubic.xml.in       \
	crashhandler.xml.in  \
	extrawm.xml.in       \
	fadedesktop.xml.in   \
	firepaint.xml.in     \
	gears.xml.in         \
	loginout.xml.in      \
	maximumize.xml.in    \
	mblur.xml.in         \
	reflex.xml.in        \
	shelf.xml.in         \
	showdesktop.xml.in   \
	splash.xml.in        \
	trailfocus.xml.in    \
	widget.xml.in        \
	wallpaper.xml.in     \
	$(animationaddonxml) \
	$(cubeaddonxml)      \
	$(groupxml)          \
	$(notificationxml)   \
	$(scalefilterxml)    \
	$(showmousexml)      \
	$(threedxml)

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

EXTRA_DIST =				\
	$(xml_in_files)			\
	animationaddon.xml.in	\
	cubeaddon.xml.in		\
	group.xml.in			\
	notification.xml.in		\
	scalefilter.xml.in		\
	showmouse.xml.in		\
	3d.xml.in

DISTCLEANFILES =	\
	$(xml_files)    \
	$(schema_files)

