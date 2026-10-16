# SPDX-License-Identifier: GPL-3.0-or-later
#
# ADR-0383: what one changing window title is allowed to cost. The caption
# and signal rate limits, the chrome title band, credential caching, and the
# end-to-end budget for a 10 Hz spinner title. Its own file because
# tests/compositor/CMakeLists.txt is past its shape budget.

set(_qindaqt_change_budget_kwin "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin")

qt_add_executable(
    qindaqt_change_rate_limiter_tests
    tst_changeratelimiter.cpp
    "${_qindaqt_change_budget_kwin}/changeratelimiter.cpp"
    "${_qindaqt_change_budget_kwin}/coalescedchangenotifier.cpp"
)
target_include_directories(
    qindaqt_change_rate_limiter_tests PRIVATE "${_qindaqt_change_budget_kwin}"
)
target_link_libraries(qindaqt_change_rate_limiter_tests PRIVATE Qt6::Core Qt6::Test)
add_test(NAME compositor.change-rate-limiter COMMAND qindaqt_change_rate_limiter_tests)
set_tests_properties(
    compositor.change-rate-limiter PROPERTIES LABELS "unit;pure;compositor"
)

qt_add_executable(
    qindaqt_chrome_overlay_raster_tests
    tst_chromeoverlayraster.cpp
    "${_qindaqt_change_budget_kwin}/chromeoverlayraster.cpp"
    "${_qindaqt_change_budget_kwin}/chromeplancomparison.cpp"
)
target_include_directories(
    qindaqt_chrome_overlay_raster_tests PRIVATE "${_qindaqt_change_budget_kwin}"
)
target_link_libraries(
    qindaqt_chrome_overlay_raster_tests PRIVATE QindaQt::HybridChrome Qt6::Gui Qt6::Test
)
add_test(NAME compositor.chrome-overlay-raster COMMAND qindaqt_chrome_overlay_raster_tests)
set_tests_properties(
    compositor.chrome-overlay-raster
    PROPERTIES
        LABELS "unit;compositor;hybrid-chrome"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(qindaqt_shell_credential_cache_tests tst_shellcredentialcache.cpp)
target_link_libraries(
    qindaqt_shell_credential_cache_tests PRIVATE QindaQt::CompositorShellActions Qt6::Test
)
add_test(
    NAME compositor.shell-credential-cache COMMAND qindaqt_shell_credential_cache_tests
)
set_tests_properties(
    compositor.shell-credential-cache PROPERTIES LABELS "unit;pure;compositor;security"
)

qt_add_executable(
    qindaqt_title_change_budget_tests
    tst_titlechangebudget.cpp
    "${_qindaqt_change_budget_kwin}/changeratelimiter.cpp"
    "${_qindaqt_change_budget_kwin}/chromeoverlayraster.cpp"
    "${_qindaqt_change_budget_kwin}/chromeplancomparison.cpp"
    "${_qindaqt_change_budget_kwin}/chromeplanlocalizer.cpp"
    "${_qindaqt_change_budget_kwin}/hybridchromeplanbuilder.cpp"
    "${_qindaqt_change_budget_kwin}/hybridstackingorder.cpp"
    "${_qindaqt_change_budget_kwin}/kwinchromehitrouting.cpp"
    "${_qindaqt_change_budget_kwin}/kwinchromemanager.cpp"
)
target_include_directories(
    qindaqt_title_change_budget_tests PRIVATE "${_qindaqt_change_budget_kwin}"
)
target_link_libraries(
    qindaqt_title_change_budget_tests
    PRIVATE
        QindaQtKWin::kwin
        QindaQt::CompositorShellActions
        QindaQt::Hybrid
        QindaQt::HybridChrome
        QindaQt::HybridConstraints
        QindaQt::HybridInput
        QindaQt::WindowManagement
        qindaqt_hybrid_chrome_pointer_router
        Qt6::Widgets
        Qt6::Test
)
add_test(NAME compositor.title-change-budget COMMAND qindaqt_title_change_budget_tests)
set_tests_properties(
    compositor.title-change-budget
    PROPERTIES
        LABELS "unit;compositor;performance"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

if(COMMAND qindaqt_enable_warnings)
    foreach(_qindaqt_change_budget_target IN ITEMS
            qindaqt_change_rate_limiter_tests
            qindaqt_chrome_overlay_raster_tests
            qindaqt_shell_credential_cache_tests
            qindaqt_title_change_budget_tests)
        qindaqt_enable_warnings("${_qindaqt_change_budget_target}")
    endforeach()
endif()
