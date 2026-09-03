# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)
find_package(Fontconfig REQUIRED)

qt_add_library(qindaqt_font_discovery STATIC)
add_library(QindaQt::FontDiscovery ALIAS qindaqt_font_discovery)

target_sources(
    qindaqt_font_discovery
    PRIVATE
        src/font_discovery.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/font_discovery/font_discovery.h
)
# AGENT-CONTRACT: Fontconfig is linked only in this module (ADR-0057). It is a
# PRIVATE link so no consumer gains fontconfig include paths or usage
# requirements; the public boundary carries plain value types only.
target_link_libraries(
    qindaqt_font_discovery
    PUBLIC QindaQt::FontPreferences Qt6::Core
    PRIVATE Fontconfig::Fontconfig
)
target_compile_features(qindaqt_font_discovery PUBLIC cxx_std_20)
set_target_properties(qindaqt_font_discovery PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_font_discovery)

install(
    TARGETS qindaqt_font_discovery
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
