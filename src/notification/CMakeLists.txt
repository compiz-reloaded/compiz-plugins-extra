include (CompizFusion)

pkg_check_modules (LIBNOTIFY_0_6 libnotify>=0.6)
if (LIBNOTIFY_0_6_FOUND)
    add_definitions (-DHAVE_LIBNOTIFY_0_6)
endif (LIBNOTIFY_0_6_FOUND)

compiz_fusion_plugin (notification PKGDEPS libnotify)
