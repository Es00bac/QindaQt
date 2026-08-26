# SPDX-License-Identifier: GPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Gui)
find_package(KDecoration3 6.6 REQUIRED CONFIG)
find_package(KF6CoreAddons 6.0 REQUIRED CONFIG)

add_library(qindaqt_decoration MODULE
    qindabutton.cpp
    qindabutton.h
    qindadecoration.cpp
    qindadecoration.h
)

target_compile_features(qindaqt_decoration PRIVATE cxx_std_20)
target_link_libraries(
    qindaqt_decoration
    PRIVATE
        Qt6::Gui
        KF6::CoreAddons
        KDecoration3::KDecoration
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
