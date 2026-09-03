# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_display_color_discovery STATIC)
add_library(QindaQt::DisplayColorDiscovery ALIAS qindaqt_display_color_discovery)

target_sources(
    qindaqt_display_color_discovery
    PRIVATE
        src/icc_text_metadata.cpp
        src/import_writer.cpp
        src/path_safety_p.h
        src/profile_discovery.cpp
        src/profile_import.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_color_discovery/profile_discovery.h
)
target_link_libraries(qindaqt_display_color_discovery PUBLIC QindaQt::DisplayColorModel Qt6::Core)
target_compile_features(qindaqt_display_color_discovery PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_color_discovery PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_color_discovery)

install(
    TARGETS qindaqt_display_color_discovery qindaqt_display_color_model
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT DisplayColorDiscoveryDevelopment
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT DisplayColorDiscoveryDevelopment
)

# Keep the unchanged C0 installed-consumer row focused too. This duplicate
# component rule is owned by C1 packaging so the C0 model implementation and
# its original install declaration remain read-only.
install(
    TARGETS qindaqt_display_color_model
    EXPORT QindaQtTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT DisplayColorModelDevelopment
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT DisplayColorModelDevelopment
)
