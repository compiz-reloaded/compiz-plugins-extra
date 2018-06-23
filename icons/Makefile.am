iconsdir = $(pluginiconsdir)/scalable/apps
icons_DATA =				\
	plugin-3d.svg			\
	plugin-addhelper.svg		\
	plugin-animationaddon.svg	\
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
	plugin-widget.svg

install-data-hook:
	$(shell which gtk-update-icon-cache &> /dev/null &&	\
	gtk-update-icon-cache -f -t $(DESTDIR)$(pluginiconsdir))
