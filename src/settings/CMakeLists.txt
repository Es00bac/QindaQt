# SPDX-License-Identifier: LGPL-3.0-or-later

qt_add_library(qindaqt_settings STATIC)
add_library(QindaQt::Settings ALIAS qindaqt_settings)

target_sources(
    qindaqt_settings
    PRIVATE
        src/layered_settings.cpp
        src/settings_document.cpp
        src/settings_migration.cpp
        src/settings_schema.cpp
        src/settings_transaction.cpp
        src/settings_types.cpp
        src/settings_value_normalizer.cpp
        src/settings_value_normalizer_p.h
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/settings/layered_settings.h
            include/qindaqt/settings/settings_document.h
            include/qindaqt/settings/settings_migration.h
            include/qindaqt/settings/settings_schema.h
            include/qindaqt/settings/settings_transaction.h
            include/qindaqt/settings/settings_types.h
)

target_link_libraries(qindaqt_settings PUBLIC Qt6::Core)
target_compile_features(qindaqt_settings PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_settings)

target_compile_definitions(
    qindaqt_settings
    PUBLIC
        QINDAQT_SETTINGS_SCHEMA_VERSION=2
)

install(
    TARGETS qindaqt_settings
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY "${PROJECT_SOURCE_DIR}/data/settings/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qindaqt/settings"
)
