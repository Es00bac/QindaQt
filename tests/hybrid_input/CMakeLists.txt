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

qt_add_executable(
    qindaqt_hybrid_input_late_shift_takeover_tests
    tst_lateshifttakeoverdetector.cpp
)
target_link_libraries(
    qindaqt_hybrid_input_late_shift_takeover_tests
    PRIVATE QindaQt::HybridInput Qt6::Test
)
qindaqt_enable_warnings(qindaqt_hybrid_input_late_shift_takeover_tests)
add_test(
    NAME hybrid.late-shift-takeover-detector
    COMMAND qindaqt_hybrid_input_late_shift_takeover_tests
)

# ADR-0282 window-management chords: modifier + left/right over a container,
# the pen's tip and barrel, and modifier + wheel roll-up.
qt_add_executable(qindaqt_hybrid_input_container_chord_tests tst_containerchords.cpp)
target_link_libraries(
    qindaqt_hybrid_input_container_chord_tests
    PRIVATE QindaQt::HybridInput Qt6::Test
)
qindaqt_enable_warnings(qindaqt_hybrid_input_container_chord_tests)
add_test(
    NAME hybrid.input-container-chords
    COMMAND qindaqt_hybrid_input_container_chord_tests
)

qt_add_executable(qindaqt_hybrid_input_wheel_roll_chord_tests tst_wheelrollchord.cpp)
target_link_libraries(
    qindaqt_hybrid_input_wheel_roll_chord_tests
    PRIVATE QindaQt::HybridInput Qt6::Test
)
qindaqt_enable_warnings(qindaqt_hybrid_input_wheel_roll_chord_tests)
add_test(
    NAME hybrid.input-wheel-roll-chord
    COMMAND qindaqt_hybrid_input_wheel_roll_chord_tests
)
