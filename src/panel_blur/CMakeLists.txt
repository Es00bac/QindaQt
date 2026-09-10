# SPDX-License-Identifier: LGPL-3.0-or-later

# Client-side blur-behind for panel surfaces. The org_kde_kwin_blur protocol
# XML is vendored (KWin-supported, LGPL-2.1); the Qt Wayland scanner generates
# the client wrapper. When the scanner or Qt WaylandClient is unavailable the
# module still compiles: every request degrades to a stored-state no-op and
# translucent panels simply render unblurred (ADR-0082).

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Quick WaylandClient)
# GuiPrivate exposes the QPA native-interface header; it may be absent in
# constrained Qt installs, in which case the blur protocol degrades to the
# no-op build below.
find_package(Qt6 6.11 QUIET COMPONENTS GuiPrivate)
find_package(Qt6 6.11 QUIET COMPONENTS WaylandScannerTools)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT IMPORTED_TARGET wayland-client)
endif()

qt_add_library(qindaqt_panel_blur STATIC)
add_library(QindaQt::PanelBlur ALIAS qindaqt_panel_blur)

target_sources(
    qindaqt_panel_blur
    PRIVATE src/panel_surface_blur.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES include/qindaqt/panel_blur/panel_surface_blur.h
)

if(TARGET Qt6::WaylandClient AND COMMAND qt6_generate_wayland_protocol_client_sources)
    # The scanner emits a C protocol header; the build tree may be CXX-only.
    enable_language(C)
    qt6_generate_wayland_protocol_client_sources(qindaqt_panel_blur
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/protocols/org-kde-kwin-blur.xml)
    # AGENT-NOTE: the generated wrapper is third-party output; SYSTEM include
    # semantics and a per-file warning waiver keep the repo's fatal warning
    # set from gating on it.
    set_source_files_properties(
        "${CMAKE_CURRENT_BINARY_DIR}/qwayland-org-kde-kwin-blur.cpp"
        PROPERTIES COMPILE_OPTIONS "-w")
    target_include_directories(qindaqt_panel_blur SYSTEM PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR})
    if(TARGET Qt6::GuiPrivate)
        target_link_libraries(qindaqt_panel_blur PUBLIC Qt6::GuiPrivate)
    else()
        # Without QPA headers the wl_surface cannot be resolved; fall back to
        # the no-op protocol build rather than failing the whole shell.
        target_compile_definitions(qindaqt_panel_blur
            PUBLIC QINDAQT_PANEL_BLUR_NO_PROTOCOL)
    endif()
else()
    target_compile_definitions(qindaqt_panel_blur
        PUBLIC QINDAQT_PANEL_BLUR_NO_PROTOCOL)
endif()

# AGENT-NOTE: GuiPrivate provides the QPA native-interface header used to
# resolve the wl_surface/wl_compositor behind a QWindow.
target_link_libraries(qindaqt_panel_blur
    PUBLIC Qt6::Gui Qt6::Quick Qt6::GuiPrivate Qt6::WaylandClient)
if(TARGET PkgConfig::WAYLAND_CLIENT)
    target_link_libraries(qindaqt_panel_blur PUBLIC PkgConfig::WAYLAND_CLIENT)
endif()
target_compile_features(qindaqt_panel_blur PUBLIC cxx_std_20)
set_target_properties(qindaqt_panel_blur PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_panel_blur)
