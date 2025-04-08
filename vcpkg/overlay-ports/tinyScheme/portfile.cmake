vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO eclipseyin147/tinyscheme
        REF a260edac12a4051b51660631152af298f5a5ec71
        SHA512 37a6eb63a1d2ec47a1f1ee0d77b1bed194b5a2cc5c33408da43ff8b6b3609ee3523c7f75e315ef646f362000a7d20cf52f93a85b47d56aeaa96e8b2dfa750d86
)

vcpkg_configure_cmake(
        SOURCE_PATH "${SOURCE_PATH}"
        PREFER_NINJA
        -DUSE_INTERFACE:Bool=TRUE
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
        PACKAGE_NAME tinyscheme
        CONFIG_PATH lib/cmake/tinyscheme
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")