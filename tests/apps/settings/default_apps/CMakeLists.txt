# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_default_applications_store_tests
    tst_default_applications_store.cpp
)
target_link_libraries(
    qindaqt_default_applications_store_tests
    PRIVATE qindaqt_settings_default_apps Qt6::Test
)
qindaqt_enable_warnings(qindaqt_default_applications_store_tests)
add_test(NAME qindaqt.settings-default-apps-store
         COMMAND qindaqt_default_applications_store_tests)
set_tests_properties(qindaqt.settings-default-apps-store PROPERTIES
    LABELS "settings;default-apps;model")

qt_add_executable(
    qindaqt_default_applications_catalog_tests
    tst_default_applications_catalog.cpp
)
target_link_libraries(
    qindaqt_default_applications_catalog_tests
    PRIVATE qindaqt_settings_default_apps QindaQt::ApplicationCatalog Qt6::Test
)
qindaqt_enable_warnings(qindaqt_default_applications_catalog_tests)
add_test(NAME qindaqt.settings-default-apps-catalog
         COMMAND qindaqt_default_applications_catalog_tests)
set_tests_properties(qindaqt.settings-default-apps-catalog PROPERTIES
    LABELS "settings;default-apps;model")

qt_add_executable(
    qindaqt_default_applications_settings_model_tests
    tst_default_applications_settings_model.cpp
)
target_link_libraries(
    qindaqt_default_applications_settings_model_tests
    PRIVATE qindaqt_settings_default_apps QindaQt::ApplicationCatalog Qt6::Test
)
qindaqt_enable_warnings(qindaqt_default_applications_settings_model_tests)
add_test(NAME qindaqt.settings-default-apps-model
         COMMAND qindaqt_default_applications_settings_model_tests)
set_tests_properties(qindaqt.settings-default-apps-model PROPERTIES
    LABELS "settings;default-apps;model")

qt_add_executable(
    qindaqt_default_applications_page_tests
    tst_default_applications_page.cpp
)
target_compile_definitions(
    qindaqt_default_applications_page_tests PRIVATE
    QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    QINDAQT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/qml"
    QINDAQT_DEFAULT_APPLICATIONS_PAGE_QML_PATH="${PROJECT_SOURCE_DIR}/src/apps/settings/default_apps/qml/DefaultApplicationsPage.qml"
)
target_link_libraries(
    qindaqt_default_applications_page_tests PRIVATE
    qindaqt_settings_default_apps qindaqt_settings_appearance
    QindaQt::ApplicationCatalog QindaQt::TokensQml
    qindaqt_themes Qt6::Quick Qt6::QuickControls2 Qt6::Test
)
add_dependencies(
    qindaqt_default_applications_page_tests qindaqt_tokens_qmlplugin
    qindaqt_controls_qmlplugin qindaqt_settings_default_apps_qmlplugin
)
qindaqt_enable_warnings(qindaqt_default_applications_page_tests)
add_test(NAME qindaqt.settings-default-apps-page
         COMMAND qindaqt_default_applications_page_tests)
set_tests_properties(qindaqt.settings-default-apps-page PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1;QT_FATAL_WARNINGS=1;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent;DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent"
    LABELS "offscreen;settings;default-apps;accessibility")

add_test(NAME qindaqt.settings-default-apps-boundary
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake")
set_tests_properties(qindaqt.settings-default-apps-boundary PROPERTIES
    LABELS "settings;default-apps;boundary")

add_test(NAME qindaqt.settings-default-apps-boundary-poison
    COMMAND "${CMAKE_COMMAND}"
            -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/boundary-poison
            -DCHECK_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary_negative.cmake")
set_tests_properties(qindaqt.settings-default-apps-boundary-poison PROPERTIES
    LABELS "settings;default-apps;boundary;poison")

add_test(NAME qindaqt.settings-default-apps-installed-route
    COMMAND "${CMAKE_COMMAND}"
            -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
            -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-default-apps-stage
            -DINSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
            -DINSTALL_DATADIR=${CMAKE_INSTALL_DATADIR}
            -DINSTALL_QMLDIR=${QT6_INSTALL_QML}
            -DSETTINGS_EXECUTABLE_NAME=qindaqt-settings${CMAKE_EXECUTABLE_SUFFIX}
            -DCONFIGURATION=$<CONFIG>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/check_installed_route.cmake")
set_tests_properties(qindaqt.settings-default-apps-installed-route PROPERTIES
    LABELS "offscreen;settings;default-apps;package" RUN_SERIAL TRUE TIMEOUT 45)

find_program(QINDAQT_DEFAULT_APPS_XDG_MIME xdg-mime REQUIRED)
add_test(NAME qindaqt.settings-default-apps-packaged-defaults
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_packaged_defaults.py"
        --build-directory "${CMAKE_BINARY_DIR}"
        --install-datadir "${CMAKE_INSTALL_DATADIR}"
        --xdg-mime "${QINDAQT_DEFAULT_APPS_XDG_MIME}")
set_tests_properties(qindaqt.settings-default-apps-packaged-defaults PROPERTIES
    LABELS "settings;default-apps;package" TIMEOUT 60)
