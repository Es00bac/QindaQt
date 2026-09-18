# SPDX-License-Identifier: GPL-3.0-or-later
# Included from tests/compositor/CMakeLists.txt inside the
# `if(TARGET qindaqt_kwin_input_adapter)` block.
# The pure touch policy (ADR-0193) lives in the pointer router library.
qt_add_executable(
    qindaqt_hybrid_chrome_touch_policy_tests
    tst_hybridchrometouchpolicy.cpp
)
target_include_directories(
    qindaqt_hybrid_chrome_touch_policy_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_hybrid_chrome_touch_policy_tests
    PRIVATE qindaqt_hybrid_chrome_pointer_router Qt6::Test
)
add_test(
    NAME compositor.hybrid-chrome-touch-policy
    COMMAND qindaqt_hybrid_chrome_touch_policy_tests
)
