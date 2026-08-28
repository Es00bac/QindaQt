# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_brightness_math_tests tst_brightness_math.cpp)
target_link_libraries(
    qindaqt_brightness_math_tests
    PRIVATE QindaQt::BrightnessModel Qt6::Test
)
qindaqt_enable_warnings(qindaqt_brightness_math_tests)
add_test(NAME qindaqt.brightness-model-math COMMAND qindaqt_brightness_math_tests)
set_tests_properties(qindaqt.brightness-model-math PROPERTIES LABELS "service;power;pure")

qt_add_executable(qindaqt_brightness_composition_tests tst_brightness_composition.cpp)
target_include_directories(
    qindaqt_brightness_composition_tests
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../power_protocol"
)
target_link_libraries(
    qindaqt_brightness_composition_tests
    PRIVATE QindaQt::BrightnessModel Qt6::Test
)
qindaqt_enable_warnings(qindaqt_brightness_composition_tests)
add_test(
    NAME qindaqt.brightness-model-composition
    COMMAND qindaqt_brightness_composition_tests
)
set_tests_properties(
    qindaqt.brightness-model-composition PROPERTIES LABELS "service;power;pure"
)

add_test(
    NAME qindaqt.brightness-model-boundary
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_boundary.cmake"
)
set_tests_properties(qindaqt.brightness-model-boundary PROPERTIES LABELS "service;power;pure;policy")
