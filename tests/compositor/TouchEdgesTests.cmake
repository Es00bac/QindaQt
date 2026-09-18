# SPDX-License-Identifier: GPL-3.0-or-later
# Included from tests/compositor/CMakeLists.txt inside the
# `if(TARGET qindaqt_kwin_input_adapter)` block.
# Touch edge mapping and reservation (ADR-0205), pure: no compositor needed.
qt_add_executable(
    qindaqt_touch_edge_actions_tests
    tst_touchedgeactions.cpp
)
target_include_directories(
    qindaqt_touch_edge_actions_tests
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/compositor/kwin"
)
target_link_libraries(
    qindaqt_touch_edge_actions_tests
    PRIVATE qindaqt_hybrid_chrome_pointer_router Qt6::Gui Qt6::Test
)
add_test(
    NAME compositor.touch-edge-actions
    COMMAND qindaqt_touch_edge_actions_tests
)
# QAction needs a QGuiApplication; the row runs on the offscreen platform.
set_tests_properties(
    compositor.touch-edge-actions
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Every key the Touch page writes has a compositor reader, and every decoded
# field is applied (ADR-0205); the poison row proves the scan bites.
add_test(NAME compositor.touch-preference-readers
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_LIST_DIR}/check_touch_preference_readers.cmake")
set_tests_properties(compositor.touch-preference-readers PROPERTIES
    LABELS "compositor;touch;boundary")
add_test(NAME compositor.touch-preference-readers-poison
    COMMAND "${CMAKE_COMMAND}" -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
            -DPOISON_ROOT=${CMAKE_CURRENT_BINARY_DIR}/touch-reader-poison
            -DCHECK_SCRIPT=${CMAKE_CURRENT_LIST_DIR}/check_touch_preference_readers.cmake
            -P "${CMAKE_CURRENT_LIST_DIR}/check_touch_preference_readers_negative.cmake")
set_tests_properties(compositor.touch-preference-readers-poison PROPERTIES
    LABELS "compositor;touch;boundary;poison")
