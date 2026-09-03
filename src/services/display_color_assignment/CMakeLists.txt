# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_display_color_assignment STATIC)
add_library(QindaQt::DisplayColorAssignment ALIAS qindaqt_display_color_assignment)

target_sources(
    qindaqt_display_color_assignment
    PRIVATE
        src/assignment_document.cpp
        src/assignment_store.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_color_assignment/assignment_document.h
            include/qindaqt/services/display_color_assignment/assignment_store.h
)
target_link_libraries(qindaqt_display_color_assignment
    PUBLIC QindaQt::DisplayColorModel QindaQt::SettingsClient Qt6::Core)
target_compile_features(qindaqt_display_color_assignment PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_color_assignment PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_color_assignment)

install(
    TARGETS qindaqt_display_color_assignment
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
