# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(
    qindaqt_container_chrome_badge_tests
    tst_container_chrome_badge.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridshadestripgeometry.cpp"
)
target_include_directories(
    qindaqt_container_chrome_badge_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_container_chrome_badge_tests
    PRIVATE
        QindaQt::HybridChrome
        QindaQt::HybridInput
        qindaqt_hybrid_chrome_pointer_router
        Qt6::Test
)
add_test(
    NAME compositor.container-chrome-badge
    COMMAND qindaqt_container_chrome_badge_tests
)
set_tests_properties(
    compositor.container-chrome-badge
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
