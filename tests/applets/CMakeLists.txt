# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_applet_manifest_tests tst_manifest.cpp)
target_link_libraries(qindaqt_applet_manifest_tests PRIVATE qindaqt_applets Qt6::Test)
target_compile_definitions(
    qindaqt_applet_manifest_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_applet_manifest_tests)
add_test(NAME qindaqt.applet-manifest COMMAND qindaqt_applet_manifest_tests)

qt_add_executable(qindaqt_applet_catalog_tests tst_catalog.cpp)
target_link_libraries(qindaqt_applet_catalog_tests PRIVATE qindaqt_applets Qt6::Test)
target_compile_definitions(
    qindaqt_applet_catalog_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_applet_catalog_tests)
add_test(NAME qindaqt.applet-catalog COMMAND qindaqt_applet_catalog_tests)
