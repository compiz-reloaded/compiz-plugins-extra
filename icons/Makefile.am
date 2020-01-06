iconsdir = $(pluginiconsdir)/scalable/apps
icons_DATA =				\
	plugin-3d.svg			\
	plugin-addhelper.svg		\
	plugin-animationaddon.svg	\
	plugin-bell.svg			\
	plugin-bench.svg		\
	plugin-bicubic.svg		\
	plugin-crashhandler.svg		\
	plugin-cubeaddon.svg		\
	plugin-extrawm.svg		\
	plugin-fadedesktop.svg		\
	plugin-firepaint.svg		\
	plugin-gears.svg		\
	plugin-grid.svg			\
	plugin-group.svg		\
	plugin-highlightcontent.svg	\
	plugin-loginout.svg		\
	plugin-maximumize.svg		\
	plugin-mblur.svg		\
	plugin-notification.svg		\
	plugin-reflex.svg		\
	plugin-scalefilter.svg		\
	plugin-shelf.svg		\
	plugin-showdesktop.svg		\
	plugin-showmouse.svg		\
	plugin-splash.svg		\
	plugin-trailfocus.svg		\
	plugin-vidcap.svg		\
	plugin-wallpaper.svg		\
	plugin-widget.svg		\
	plugin-workspacenames.svg

EXTRA_DIST = \
	$(icons_DATA)

install-data-hook:
	if test x$(DESTDIR) = x; then \
		$(UPDATE_ICON_CACHE) -f -t $(pluginiconsdir); \
	fi

uninstall-local:
	if test x$(DESTDIR) = x; then \
		rm $(pluginiconsdir)/icon-theme.cache; \
	fi
