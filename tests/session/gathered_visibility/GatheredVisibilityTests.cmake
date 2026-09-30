# SPDX-License-Identifier: GPL-3.0-or-later
find_package(LayerShellQt 6.6.6 REQUIRED)
qt_add_executable(qindaqt_gathered_mock_panel
    "${CMAKE_CURRENT_LIST_DIR}/mock_reservation_panel.cpp")
target_link_libraries(qindaqt_gathered_mock_panel
    PRIVATE LayerShellQt::Interface Qt6::Gui)
set_target_properties(qindaqt_gathered_mock_panel PROPERTIES CXX_EXTENSIONS OFF)

add_test(
    NAME compositor.gathered-visibility.mixed-pager.single-640x480
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_LIST_DIR}/../shade_visibility/run_shade_visibility.py"
        --launcher "$<TARGET_FILE:qindaqt-wm>"
        --plugin-root "${_qindaqt_test_plugin_root}"
        --kwin "${QINDAQT_KWIN_WAYLAND}"
        --scenario "${PROJECT_SOURCE_DIR}/tests/scenarios/gathered-640x480.json"
        --flow gathered
        --panel-fixture "$<TARGET_FILE:qindaqt_gathered_mock_panel>"
        --kind gtk-ssd
        --output-root "${CMAKE_BINARY_DIR}/sv"
)
set_tests_properties(
    compositor.gathered-visibility.mixed-pager.single-640x480
    PROPERTIES TIMEOUT 480 SKIP_RETURN_CODE 77 RUN_SERIAL TRUE
               LABELS "integration;compositor;hybrid;gathered;pixels"
)
