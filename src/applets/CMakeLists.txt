# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_applets STATIC)

target_sources(
    qindaqt_applets
    PRIVATE
        src/api_version.cpp
        src/applet_manifest.cpp
        src/manifest_catalog.cpp
        src/manifest_loader.cpp
        src/manifest_types.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/applets/api_version.h
            include/qindaqt/applets/applet_manifest.h
            include/qindaqt/applets/manifest_catalog.h
            include/qindaqt/applets/manifest_loader.h
            include/qindaqt/applets/manifest_types.h
)

target_link_libraries(qindaqt_applets PUBLIC Qt6::Core)
target_compile_features(qindaqt_applets PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_applets)

install(
    TARGETS qindaqt_applets
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/applets/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
)

# AGENT-CONTRACT: additive per-consumer COMPONENT, mirroring the TextEditor
# precedent in src/controls/CMakeLists.txt. The Clipboard applet's packaged
# component (tests/shell/clipboard_applet, --component ClipboardApplet) must
# carry its own manifest; this registry is the manifest's owning install rule.
install(
    FILES "${PROJECT_SOURCE_DIR}/data/applets/clipboard.json"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/applets"
    COMPONENT ClipboardApplet
)
