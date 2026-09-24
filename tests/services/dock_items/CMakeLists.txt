# SPDX-License-Identifier: LGPL-3.0-or-later

# ADR-0265: the pure dock value and the public pin helper. Neither test
# reaches a bus, a display, or the user's settings.
qt_add_executable(qindaqt_dock_items_tests tst_dock_items.cpp)
target_link_libraries(qindaqt_dock_items_tests PRIVATE QindaQt::DockItems Qt6::Test)
qindaqt_enable_warnings(qindaqt_dock_items_tests)
add_test(NAME qindaqt.services-dock-items COMMAND qindaqt_dock_items_tests)
set_tests_properties(qindaqt.services-dock-items PROPERTIES
    LABELS "services;dock;settings")

qt_add_executable(qindaqt_dock_pins_tests tst_settings1_dock_pins.cpp)
target_link_libraries(qindaqt_dock_pins_tests PRIVATE QindaQt::DockPins Qt6::Test)
qindaqt_enable_warnings(qindaqt_dock_pins_tests)
add_test(NAME qindaqt.services-dock-pins COMMAND qindaqt_dock_pins_tests)
set_tests_properties(qindaqt.services-dock-pins PROPERTIES
    LABELS "services;dock;settings"
    ENVIRONMENT_MODIFICATION "DBUS_SESSION_BUS_ADDRESS=unset:")
