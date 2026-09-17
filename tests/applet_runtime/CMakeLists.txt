# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_applet_instance_resolver_tests
    tst_applet_instance_resolver.cpp
)
target_link_libraries(
    qindaqt_applet_instance_resolver_tests
    PRIVATE QindaQt::AppletRuntime Qt6::Test
)
target_compile_definitions(
    qindaqt_applet_instance_resolver_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_applet_instance_resolver_tests)
add_test(
    NAME qindaqt.applet-runtime-resolution
    COMMAND qindaqt_applet_instance_resolver_tests
)

qt_add_executable(
    qindaqt_applet_manifest_minimum_tests
    tst_applet_manifest_minimum.cpp
)
target_link_libraries(
    qindaqt_applet_manifest_minimum_tests
    PRIVATE QindaQt::AppletRuntime Qt6::Test
)
target_compile_definitions(
    qindaqt_applet_manifest_minimum_tests
    PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)
qindaqt_enable_warnings(qindaqt_applet_manifest_minimum_tests)
add_test(
    NAME qindaqt.applet-runtime-manifest-minimum
    COMMAND qindaqt_applet_manifest_minimum_tests
)
