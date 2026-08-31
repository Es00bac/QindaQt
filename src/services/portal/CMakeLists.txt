# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui DBus)

qt_add_library(qindaqt_portal_appearance STATIC)
add_library(QindaQt::PortalAppearance ALIAS qindaqt_portal_appearance)
target_sources(
    qindaqt_portal_appearance
    PRIVATE
        src/appearance_policy.cpp
        src/appearance_theme_catalog.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/portal/appearance_policy.h
            include/qindaqt/services/portal/appearance_theme_catalog.h
)
target_link_libraries(
    qindaqt_portal_appearance
    PUBLIC QindaQt::DesignTokens qindaqt_themes Qt6::Core Qt6::Gui
)
target_compile_features(qindaqt_portal_appearance PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_portal_appearance PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_portal_appearance PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_portal_appearance)

qt_add_library(qindaqt_portal_service STATIC)
add_library(QindaQt::PortalService ALIAS qindaqt_portal_service)
target_sources(
    qindaqt_portal_service
    PRIVATE
        src/portal_dbus_types_p.h
        src/portal_settings_object.cpp
        src/portal_settings_object_p.h
        src/resident_portal_service.cpp
        src/settings1_appearance_source.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/portal/appearance_source.h
            include/qindaqt/services/portal/resident_portal_service.h
            include/qindaqt/services/portal/settings1_appearance_source.h
)
target_link_libraries(
    qindaqt_portal_service
    PUBLIC QindaQt::PortalAppearance QindaQt::SettingsClient Qt6::Core Qt6::DBus
)
target_compile_features(qindaqt_portal_service PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_portal_service PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_portal_service PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_portal_service)

qt_add_executable(xdg-desktop-portal-qindaqt app/main.cpp)
target_link_libraries(
    xdg-desktop-portal-qindaqt
    PRIVATE QindaQt::PortalService QindaQt::SettingsClient Qt6::Core Qt6::DBus
)
target_compile_definitions(
    xdg-desktop-portal-qindaqt PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
target_compile_features(xdg-desktop-portal-qindaqt PRIVATE cxx_std_20)
target_compile_definitions(xdg-desktop-portal-qindaqt PRIVATE QT_NO_KEYWORDS)
set_target_properties(xdg-desktop-portal-qindaqt PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(xdg-desktop-portal-qindaqt)

configure_file(
    data/org.freedesktop.impl.portal.desktop.qindaqt.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.impl.portal.desktop.qindaqt.service"
    @ONLY
)
configure_file(
    data/xdg-desktop-portal-qindaqt.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/xdg-desktop-portal-qindaqt.service"
    @ONLY
)

install(
    TARGETS qindaqt_portal_appearance qindaqt_portal_service
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT QindaQtPortalP0
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT QindaQtPortalP0
)
install(
    TARGETS xdg-desktop-portal-qindaqt
    RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}"
    COMPONENT QindaQtPortalP0
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/org.freedesktop.impl.portal.desktop.qindaqt.service"
    DESTINATION "${KDE_INSTALL_DBUSSERVICEDIR}"
    COMPONENT QindaQtPortalP0
)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/xdg-desktop-portal-qindaqt.service"
    DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
    COMPONENT QindaQtPortalP0
)
install(
    FILES data/qindaqt.portal
    DESTINATION "${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal/portals"
    COMPONENT QindaQtPortalP0
)
install(
    FILES data/qindaqt-portals.conf
    DESTINATION "${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal"
    COMPONENT QindaQtPortalP0
)
# The resident executable derives the standard accent value from installed QST
# themes. Keep that runtime payload in the same narrow package component so a
# Settings-only backend cannot activate with an empty policy catalog.
install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/themes/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
    COMPONENT QindaQtPortalP0
)
