# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_settings_schema_tests tst_settings_schema.cpp)
target_link_libraries(qindaqt_settings_schema_tests PRIVATE qindaqt_settings Qt6::Test)
target_compile_definitions(qindaqt_settings_schema_tests PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
qindaqt_enable_warnings(qindaqt_settings_schema_tests)
add_test(NAME qindaqt.settings-schema COMMAND qindaqt_settings_schema_tests)

qt_add_executable(qindaqt_layered_settings_tests tst_layered_settings.cpp)
target_link_libraries(qindaqt_layered_settings_tests PRIVATE qindaqt_settings Qt6::Test)
target_compile_definitions(qindaqt_layered_settings_tests PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
qindaqt_enable_warnings(qindaqt_layered_settings_tests)
add_test(NAME qindaqt.settings-layers COMMAND qindaqt_layered_settings_tests)

qt_add_executable(qindaqt_settings_persistence_tests tst_settings_persistence.cpp)
target_link_libraries(qindaqt_settings_persistence_tests PRIVATE qindaqt_settings Qt6::Test)
target_compile_definitions(
    qindaqt_settings_persistence_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_settings_persistence_tests)
add_test(NAME qindaqt.settings-persistence COMMAND qindaqt_settings_persistence_tests)

qt_add_executable(qindaqt_settings_migration_tests tst_settings_migration.cpp)
target_link_libraries(qindaqt_settings_migration_tests PRIVATE qindaqt_settings Qt6::Test)
target_compile_definitions(
    qindaqt_settings_migration_tests
    PRIVATE
        QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_settings_migration_tests)
add_test(NAME qindaqt.settings-migration COMMAND qindaqt_settings_migration_tests)
