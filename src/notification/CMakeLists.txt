include (CompizFusion)

pkg_check_modules (LIBNOTIFY_0_6_1 libnotify>=0.6.1)
if (LIBNOTIFY_0_6_1_FOUND)
    add_definitions (-DHAVE_LIBNOTIFY_0_6_1)
endif (LIBNOTIFY_0_6_1_FOUND)

compiz_fusion_plugin (notification PKGDEPS libnotify)
