xmldir = $(metadatadir)

if ANIMATIONADDON_PLUGIN
animationaddonxml = animationaddon.xml.in
endif

if CUBEADDON_PLUGIN
cubeaddonxml = cubeaddon.xml.in
endif

if BELL_PLUGIN
bellxml = bell.xml.in
endif

if GROUP_PLUGIN
groupxml = group.xml.in
endif

if NOTIFICATION_PLUGIN
notificationxml = notification.xml.in
endif

if SCALEFILTER_PLUGIN
scalefilterxml = scalefilter.xml.in
endif

if SHOWMOUSE_PLUGIN
showmousexml = showmouse.xml.in
endif

if HIGHLIGHTCONTENT_PLUGIN
highlightcontentxml = highlightcontent.xml.in
endif

if THREED_PLUGIN
threedxml = 3d.xml.in
endif

if WORKSPACENAMES_PLUGIN
# Depends on Text
workspacenamesxml = workspacenames.xml.in
endif

xml_in_files =                 \
	addhelper.xml.in       \
	bench.xml.in           \
	bicubic.xml.in         \
	crashhandler.xml.in    \
	extrawm.xml.in         \
	fadedesktop.xml.in     \
	firepaint.xml.in       \
	gears.xml.in           \
	grid.xml.in            \
	loginout.xml.in        \
	maximumize.xml.in      \
	mblur.xml.in           \
	reflex.xml.in          \
	shelf.xml.in           \
	showdesktop.xml.in     \
	splash.xml.in          \
	trailfocus.xml.in      \
	widget.xml.in          \
	vidcap.xml.in          \
	wallpaper.xml.in       \
	$(animationaddonxml)   \
	$(cubeaddonxml)        \
	$(bellxml)             \
	$(groupxml)            \
	$(notificationxml)     \
	$(scalefilterxml)      \
	$(showmousexml)        \
	$(highlightcontentxml) \
	$(threedxml)           \
	$(workspacenamesxml)

xml_files    = $(xml_in_files:.xml.in=.xml)
xml_DATA     = $(xml_files)

%.xml: %.xml.in
	GETTEXTDATADIR=$(top_srcdir)/gettext $(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST =                    \
	$(xml_in_files)         \
	animationaddon.xml.in   \
	cubeaddon.xml.in        \
	bell.xml.in             \
	group.xml.in            \
	notification.xml.in     \
	scalefilter.xml.in      \
	showmouse.xml.in        \
	highlightcontent.xml.in \
	3d.xml.in               \
	workspacenames.xml.in

CLEANFILES =                    \
	$(xml_files)
