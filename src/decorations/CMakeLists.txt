# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Gui Widgets)
# The decoration library is qindaqt-kwin's vendored copy (ADR-0289); its
# package names the fork's decoration namespace as KDECORATION_PLUGIN_DIR.
include("${PROJECT_SOURCE_DIR}/src/compositor/cmake/QindaQtKWinAbi.cmake")
qindaqt_resolve_kwin_abi(
    "${PROJECT_SOURCE_DIR}/compositor/upstream/kwin.json"
    _qindaqt_decoration_abi_version
)
find_package(QindaQtKWinDecoration ${_qindaqt_decoration_abi_version} EXACT REQUIRED CONFIG)
find_package(KF6CoreAddons 6.0 REQUIRED CONFIG)

add_library(qindaqt_decoration_visuals STATIC
    qindadecorationvisuals.cpp
    qindadecorationvisuals.h
)
target_compile_features(qindaqt_decoration_visuals PUBLIC cxx_std_20)
target_include_directories(qindaqt_decoration_visuals PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(
    qindaqt_decoration_visuals
    PUBLIC
        QindaQt::DecorationPainter
        Qt6::Gui
        QindaQtKWinDecoration::KDecoration
)
set_target_properties(qindaqt_decoration_visuals PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_decoration_visuals)

add_library(qindaqt_decoration_context_menu STATIC
    qindawindowcontextmenu.cpp
    qindawindowcontextmenu.h
)
target_compile_features(qindaqt_decoration_context_menu PUBLIC cxx_std_20)
target_include_directories(qindaqt_decoration_context_menu PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(qindaqt_decoration_context_menu PUBLIC Qt6::Widgets)
set_target_properties(qindaqt_decoration_context_menu PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)
qindaqt_enable_warnings(qindaqt_decoration_context_menu)

add_library(qindaqt_decoration MODULE
    qindabutton.cpp
    qindabutton.h
    qindadecoration.cpp
    qindadecorationgeometry.cpp
    qindadecorationinput.cpp
    qindadecoration.h
)

target_compile_features(qindaqt_decoration PRIVATE cxx_std_20)
target_link_libraries(
    qindaqt_decoration
    PRIVATE
        Qt6::Gui
        KF6::CoreAddons
        QindaQtKWinDecoration::KDecoration
        qindaqt_decoration_context_menu
        qindaqt_decoration_visuals
)
set_target_properties(
    qindaqt_decoration
    PROPERTIES
        CXX_EXTENSIONS OFF
        OUTPUT_NAME org.qindaqt
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/${KDECORATION_PLUGIN_DIR}"
        PREFIX ""
)
qindaqt_enable_warnings(qindaqt_decoration)

install(
    TARGETS qindaqt_decoration
    DESTINATION "${KDE_INSTALL_PLUGINDIR}/${KDECORATION_PLUGIN_DIR}"
)
