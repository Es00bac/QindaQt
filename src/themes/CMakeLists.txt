# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_themes STATIC)

target_sources(
    qindaqt_themes
    PRIVATE
        src/theme_catalog.cpp
        src/theme_loader.cpp
        src/theme_spec.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/themes/theme_catalog.h
            include/qindaqt/themes/theme_loader.h
            include/qindaqt/themes/theme_spec.h
)

target_link_libraries(qindaqt_themes PUBLIC Qt6::Core Qt6::Gui)
target_compile_features(qindaqt_themes PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_themes)

install(
    TARGETS qindaqt_themes
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/themes/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/themes"
)
