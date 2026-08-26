# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_hybrid_input_tests tst_interactioncontroller.cpp)
target_link_libraries(qindaqt_hybrid_input_tests PRIVATE QindaQt::HybridInput Qt6::Test)
qindaqt_enable_warnings(qindaqt_hybrid_input_tests)
add_test(NAME hybrid.interaction-controller COMMAND qindaqt_hybrid_input_tests)

qt_add_executable(qindaqt_hybrid_input_keyboard_dock_tests tst_keyboarddock.cpp)
target_link_libraries(
    qindaqt_hybrid_input_keyboard_dock_tests
    PRIVATE QindaQt::HybridInput Qt6::Test
)
qindaqt_enable_warnings(qindaqt_hybrid_input_keyboard_dock_tests)
add_test(
    NAME hybrid.input-keyboard-dock
    COMMAND qindaqt_hybrid_input_keyboard_dock_tests
)

qt_add_executable(qindaqt_hybrid_input_keyboard_geometry_tests tst_keyboardgeometry.cpp)
target_link_libraries(
    qindaqt_hybrid_input_keyboard_geometry_tests
    PRIVATE QindaQt::HybridInput Qt6::Test
)
qindaqt_enable_warnings(qindaqt_hybrid_input_keyboard_geometry_tests)
add_test(
    NAME hybrid.input-keyboard-geometry
    COMMAND qindaqt_hybrid_input_keyboard_geometry_tests
)
