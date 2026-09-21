# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus)
find_package(PkgConfig REQUIRED)

# AGENT-CONTRACT: the X11 dependency of the tray proxy is confined to this
# module (ADR-0229). The shell, the tray applet, and the compositor gain no
# xcb link edge from this change.
pkg_check_modules(
    XCB
    REQUIRED
    IMPORTED_TARGET
    xcb
    xcb-composite
    xcb-damage
    xcb-shape
    xcb-xtest
    xcb-xfixes
)

qt_add_library(qindaqt_xembed_tray_proxy STATIC)
add_library(QindaQt::XEmbedTrayProxy ALIAS qindaqt_xembed_tray_proxy)

target_sources(
    qindaqt_xembed_tray_proxy
    PRIVATE
        src/tray_icon_image.cpp
        src/tray_proxy_coordinator.cpp
        src/sni/sni_image_types.cpp
        src/sni/sni_image_types.h
        src/sni/status_notifier_item.cpp
        src/sni/status_notifier_item.h
        src/x11/xcb_embedded_icon.cpp
        src/x11/xcb_embedded_icon.h
        src/x11/xcb_tray_backend.cpp
        src/x11/xcb_tray_backend.h
        src/x11/xcb_tray_visual.cpp
        src/x11/xcb_tray_visual.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/xembed_tray_proxy/tray_backend.h
            include/qindaqt/services/xembed_tray_proxy/tray_icon_image.h
            include/qindaqt/services/xembed_tray_proxy/tray_proxy_coordinator.h
)
target_include_directories(qindaqt_xembed_tray_proxy PRIVATE src)
target_link_libraries(
    qindaqt_xembed_tray_proxy
    PUBLIC Qt6::Core Qt6::DBus
    PRIVATE PkgConfig::XCB
)
target_compile_features(qindaqt_xembed_tray_proxy PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_xembed_tray_proxy PRIVATE QT_NO_KEYWORDS)
set_target_properties(qindaqt_xembed_tray_proxy PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_xembed_tray_proxy)

install(
    TARGETS qindaqt_xembed_tray_proxy
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

qt_add_executable(qindaqt-xembed-tray-proxy app/main.cpp)
target_link_libraries(
    qindaqt-xembed-tray-proxy
    PRIVATE QindaQt::XEmbedTrayProxy Qt6::Core Qt6::DBus
)
target_compile_definitions(
    qindaqt-xembed-tray-proxy PRIVATE QINDAQT_VERSION="${PROJECT_VERSION}"
)
set_target_properties(qindaqt-xembed-tray-proxy PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt-xembed-tray-proxy)

configure_file(
    data/qindaqt-xembed-tray-proxy.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-xembed-tray-proxy.service"
    @ONLY
)

install(TARGETS qindaqt-xembed-tray-proxy RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/qindaqt-xembed-tray-proxy.service"
    DESTINATION "${KDE_INSTALL_SYSTEMDUSERUNITDIR}"
)
