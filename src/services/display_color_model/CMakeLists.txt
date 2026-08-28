# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_display_color_model STATIC)
add_library(QindaQt::DisplayColorModel ALIAS qindaqt_display_color_model)

target_sources(
    qindaqt_display_color_model
    PRIVATE
        src/color_model.cpp
        src/color_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/display_color_model/color_limits.h
            include/qindaqt/services/display_color_model/color_model.h
            include/qindaqt/services/display_color_model/color_types.h
            include/qindaqt/services/display_color_model/color_validation.h
)
target_link_libraries(qindaqt_display_color_model PUBLIC Qt6::Core)
target_compile_features(qindaqt_display_color_model PUBLIC cxx_std_20)
set_target_properties(qindaqt_display_color_model PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_display_color_model)

install(
    TARGETS qindaqt_display_color_model
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
