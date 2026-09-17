# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(qindaqt_smart_lights_store_tests tst_smart_lights_store.cpp)
target_link_libraries(
    qindaqt_smart_lights_store_tests PRIVATE QindaQt::SmartLightsStore Qt6::Test
)
qindaqt_enable_warnings(qindaqt_smart_lights_store_tests)
add_test(NAME qindaqt.smart-lights-store COMMAND qindaqt_smart_lights_store_tests)
set_tests_properties(
    qindaqt.smart-lights-store PROPERTIES LABELS "service;smart-lights;pure"
)
