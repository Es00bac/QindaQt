# SPDX-License-Identifier: GPL-3.0-or-later

# Iconified-window rows (ADR-0203): the pure controller and the chip pointer
# router. Included from tests/compositor/CMakeLists.txt inside the KWin block,
# next to the shade controller they mirror, so the registry file stays short.

qt_add_executable(
    qindaqt_hybrid_iconify_controller_tests
    tst_hybridiconifycontroller.cpp
    "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin/hybridiconifycontroller.cpp"
)
target_include_directories(
    qindaqt_hybrid_iconify_controller_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_hybrid_iconify_controller_tests
    PRIVATE Qt6::Core Qt6::Test
)
add_test(
    NAME compositor.hybrid-iconify-controller
    COMMAND qindaqt_hybrid_iconify_controller_tests
)

qt_add_executable(
    qindaqt_hybrid_icon_chip_router_tests
    tst_hybridiconchiprouter.cpp
)
target_compile_features(qindaqt_hybrid_icon_chip_router_tests PRIVATE cxx_std_20)
target_link_libraries(
    qindaqt_hybrid_icon_chip_router_tests
    PRIVATE qindaqt_hybrid_chrome_pointer_router Qt6::Test
)
add_test(
    NAME compositor.hybrid-icon-chip-router
    COMMAND qindaqt_hybrid_icon_chip_router_tests
)
