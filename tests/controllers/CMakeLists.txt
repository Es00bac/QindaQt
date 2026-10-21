# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(qindaqt_controller_policy_test tst_controller_policy.cpp)
target_link_libraries(qindaqt_controller_policy_test PRIVATE qindaqt_controller_policy Qt6::Test)
add_test(NAME qindaqt.controller-policy COMMAND qindaqt_controller_policy_test)
set_tests_properties(qindaqt.controller-policy PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

# ADR-0428: which holders of a controller node pause desktop control for the
# focused application, over fake /proc trees (no live process is read).
add_executable(qindaqt_game_priority_focus_test tst_game_priority_focus.cpp fake_proc_tree.h)
target_include_directories(qindaqt_game_priority_focus_test PRIVATE ../../src/controllers)
target_link_libraries(qindaqt_game_priority_focus_test PRIVATE qindaqt_controller_policy Qt6::Test)
add_test(NAME qindaqt.controller-focus-ownership COMMAND qindaqt_game_priority_focus_test)
set_tests_properties(qindaqt.controller-focus-ownership PROPERTIES TIMEOUT 30)

add_executable(qindaqt_controller_input_state_test tst_held_input_state.cpp)
target_include_directories(qindaqt_controller_input_state_test PRIVATE ../../src/controllers)
target_link_libraries(qindaqt_controller_input_state_test PRIVATE qindaqt_controller_policy Qt6::Test)
add_test(NAME qindaqt.controller-input-state COMMAND qindaqt_controller_input_state_test)
set_tests_properties(qindaqt.controller-input-state PROPERTIES TIMEOUT 15)

add_library(qindaqt_controller_settings_test_support STATIC
    ../../src/apps/settings/input/controllers_model.cpp
    ../../src/apps/settings/input/controller_pages_model.cpp
    ../../src/apps/settings/input/input_macros_model.cpp
    ../../src/apps/settings/input/include/qindaqt/apps/settings_input/input_macros_model.h
    ../../src/apps/settings/input/qt_controller_port.cpp
    ../../src/apps/settings/input/include/qindaqt/apps/settings_input/controllers_model.h
    ../../src/apps/settings/input/include/qindaqt/apps/settings_input/controller_port.h
    ../../src/apps/settings/input/include/qindaqt/apps/settings_input/json_input_port.h)
target_include_directories(qindaqt_controller_settings_test_support PUBLIC ../../src/apps/settings/input/include)
target_link_libraries(qindaqt_controller_settings_test_support PUBLIC qindaqt_controller_policy Qt6::DBus)
add_executable(qindaqt_controller_settings_test tst_controller_settings.cpp)
target_link_libraries(qindaqt_controller_settings_test PRIVATE qindaqt_controller_settings_test_support Qt6::Test)
add_test(NAME qindaqt.controller-settings COMMAND qindaqt_controller_settings_test)
set_tests_properties(qindaqt.controller-settings PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

add_executable(qindaqt_controller_bus_test tst_controller_bus.cpp
    ../../src/controllers/kwin/voice_button.cpp ../../src/controllers/kwin/voice_button.h)
target_include_directories(qindaqt_controller_bus_test PRIVATE ../../src/controllers/kwin)
target_link_libraries(qindaqt_controller_bus_test PRIVATE qindaqt_controller_settings_test_support Qt6::Test)
find_program(QINDAQT_CONTROLLER_DBUS_RUN_SESSION dbus-run-session REQUIRED)
add_test(NAME qindaqt.controller-bus COMMAND ${QINDAQT_CONTROLLER_DBUS_RUN_SESSION} -- $<TARGET_FILE:qindaqt_controller_bus_test>)
set_tests_properties(qindaqt.controller-bus PROPERTIES TIMEOUT 15)

if(TARGET qindaqt_controller_runtime)
    add_executable(qindaqt_game_priority_observer_test tst_game_priority_observer.cpp)
    target_include_directories(qindaqt_game_priority_observer_test PRIVATE ../../src/controllers)
    target_link_libraries(qindaqt_game_priority_observer_test PRIVATE qindaqt_controller_runtime Qt6::Test)
    add_test(NAME qindaqt.controller-observation COMMAND qindaqt_game_priority_observer_test)
    set_tests_properties(qindaqt.controller-observation PROPERTIES TIMEOUT 15)
    add_executable(qindaqt_controller_runtime_test tst_controller_runtime.cpp)
    target_link_libraries(qindaqt_controller_runtime_test PRIVATE qindaqt_controller_runtime Qt6::Test)
    add_test(NAME qindaqt.controller-runtime COMMAND qindaqt_controller_runtime_test)
    set_tests_properties(qindaqt.controller-runtime PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
endif()

add_executable(qindaqt_controller_pages_policy_test tst_controller_pages_policy.cpp)
target_link_libraries(qindaqt_controller_pages_policy_test PRIVATE qindaqt_controller_policy Qt6::Test)
add_test(NAME qindaqt.controller-pages-policy COMMAND qindaqt_controller_pages_policy_test)
set_tests_properties(qindaqt.controller-pages-policy PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 15)

add_executable(qindaqt_controller_text_test tst_controller_text.cpp)
target_include_directories(qindaqt_controller_text_test PRIVATE ../../src/controllers)
target_link_libraries(qindaqt_controller_text_test PRIVATE qindaqt_controller_policy Qt6::Test)
add_test(NAME qindaqt.controller-text COMMAND qindaqt_controller_text_test)
set_tests_properties(qindaqt.controller-text PROPERTIES TIMEOUT 15)

# The focused SDK fixture uses the installed QST facade/theme and the real
# source QML, without linking the rest of the Settings application.
find_package(Qt6 6.11 QUIET COMPONENTS QuickTest QuickControls2)
find_library(QINDAQT_CONTROLLER_TEST_TOKENS qindaqt_tokens_qml PATHS /usr/lib64/qt6/qml/QindaQt/Tokens /usr/lib/qt6/qml/QindaQt/Tokens)
find_library(QINDAQT_CONTROLLER_TEST_THEMES qindaqt_themes)
if(TARGET Qt6::QuickTest AND QINDAQT_CONTROLLER_TEST_TOKENS AND QINDAQT_CONTROLLER_TEST_THEMES)
    add_executable(qindaqt_controller_editor_test controller_qml_setup.cpp)
    target_link_libraries(qindaqt_controller_editor_test PRIVATE qindaqt_controller_settings_test_support Qt6::QuickTest Qt6::QuickControls2 ${QINDAQT_CONTROLLER_TEST_TOKENS} ${QINDAQT_CONTROLLER_TEST_THEMES})
    add_test(NAME qindaqt.controller-editor COMMAND qindaqt_controller_editor_test -input ${CMAKE_CURRENT_SOURCE_DIR}/qml)
    set_tests_properties(qindaqt.controller-editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software" TIMEOUT 30)
endif()

add_executable(qindaqt_button_layers_test tst_button_layers.cpp)
target_include_directories(qindaqt_button_layers_test PRIVATE ../../src/controllers)
target_link_libraries(qindaqt_button_layers_test PRIVATE qindaqt_controller_policy Qt6::Test)
add_test(NAME qindaqt.button-layers COMMAND qindaqt_button_layers_test)
set_tests_properties(qindaqt.button-layers PROPERTIES TIMEOUT 15)

# Keep the new Macros destination/search metadata aligned with the real route.
get_filename_component(QINDAQT_CONTROLLER_CHECK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(QINDAQT_CONTROLLER_NAVIGATION "${QINDAQT_CONTROLLER_CHECK_ROOT}/src/apps/settings_center")
add_executable(qindaqt_controller_route_search_test
    ../apps/settings_center/tst_settings_route_search.cpp
    ${QINDAQT_CONTROLLER_NAVIGATION}/settings_route.cpp
    ${QINDAQT_CONTROLLER_NAVIGATION}/settings_route_registry.cpp
    ${QINDAQT_CONTROLLER_NAVIGATION}/settings_route_search_metadata.cpp
    ${QINDAQT_CONTROLLER_NAVIGATION}/settings_navigation_controller.cpp
    ${QINDAQT_CONTROLLER_NAVIGATION}/settings_launch_arguments.cpp
    ${QINDAQT_CONTROLLER_NAVIGATION}/settings_navigation_controller.h)
target_include_directories(qindaqt_controller_route_search_test PRIVATE ${QINDAQT_CONTROLLER_CHECK_ROOT} ${QINDAQT_CONTROLLER_NAVIGATION})
target_link_libraries(qindaqt_controller_route_search_test PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(qindaqt_controller_route_search_test PRIVATE QINDAQT_SOURCE_DIR="${QINDAQT_CONTROLLER_CHECK_ROOT}")
add_test(NAME qindaqt.controller-route-search COMMAND qindaqt_controller_route_search_test)
set_tests_properties(qindaqt.controller-route-search PROPERTIES TIMEOUT 15)
