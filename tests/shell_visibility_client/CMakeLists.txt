# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_compositor_visibility_client_tests
    tst_compositor_visibility_client.cpp
)
target_link_libraries(
    qindaqt_compositor_visibility_client_tests
    PRIVATE QindaQt::ShellVisibilityClient Qt6::Test
)
qindaqt_enable_warnings(qindaqt_compositor_visibility_client_tests)
add_test(
    NAME qindaqt.shell-visibility-client
    COMMAND qindaqt_compositor_visibility_client_tests
)

qt_add_executable(
    qindaqt_qt_compositor_visibility_transport_tests
    tst_qt_compositor_visibility_transport.cpp
)
target_link_libraries(
    qindaqt_qt_compositor_visibility_transport_tests
    PRIVATE QindaQt::ShellVisibilityClient Qt6::Test
)
qindaqt_enable_warnings(qindaqt_qt_compositor_visibility_transport_tests)
add_test(
    NAME qindaqt.shell-visibility-qt-transport
    COMMAND qindaqt_qt_compositor_visibility_transport_tests
)
