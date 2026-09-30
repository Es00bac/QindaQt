# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 REQUIRED COMPONENTS Core Test)
foreach(behavior secure_buffer collection_store storage_failures storage_decoding)
    add_executable(tst_keyring_${behavior} tst_${behavior}.cpp)
    set_target_properties(tst_keyring_${behavior} PROPERTIES AUTOMOC ON)
    target_link_libraries(tst_keyring_${behavior} PRIVATE QindaQt::KeyringStorage Qt6::Core Qt6::Test)
    add_test(NAME keyring_${behavior} COMMAND tst_keyring_${behavior})
endforeach()

target_include_directories(tst_keyring_storage_decoding PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/keyring/src")

if(TARGET qindaqt_keyring_service)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(TARGET QindaQt::SessionSupervisorSupport)
    add_executable(qindaqt_keyring_session_owner_helper session_owner_helper.cpp)
    target_include_directories(qindaqt_keyring_session_owner_helper PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(qindaqt_keyring_session_owner_helper PRIVATE QindaQt::SessionSupervisorSupport Qt6::Core)
    qindaqt_enable_warnings(qindaqt_keyring_session_owner_helper)
    endif()
    add_test(NAME keyring_secret_service COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/test_secret_service.py" "$<TARGET_FILE:qindaqt-keyring>")
    if(NOT TARGET QindaQt::SessionSupervisorSupport)
        set_tests_properties(keyring_secret_service PROPERTIES ENVIRONMENT "QINDAQT_KEYRING_OWNER_HELPER=disabled")
    endif()
    set_tests_properties(keyring_secret_service PROPERTIES TIMEOUT 120 LABELS "keyring;dbus;private-fixture")
endif()

if(TARGET qindaqt-keyring-prompt)
    find_package(QindaTK CONFIG REQUIRED)
    add_executable(tst_keyring_prompt_qml tst_prompt_qml.cpp
        "${PROJECT_SOURCE_DIR}/src/services/keyring/prompt/prompt_controller.cpp"
        "${PROJECT_SOURCE_DIR}/src/services/keyring/prompt/prompt_controller.h")
    target_include_directories(tst_keyring_prompt_qml PRIVATE "${PROJECT_SOURCE_DIR}")
    target_link_libraries(tst_keyring_prompt_qml PRIVATE qindaqt_keyring_service
        QindaQt::AuthenticationOverlay QindaTK::qindatk Qt6::Test Qt6::Qml Qt6::Quick)
    target_compile_definitions(tst_keyring_prompt_qml PRIVATE
        QINDAQT_KEYRING_PROMPT_QML="${PROJECT_SOURCE_DIR}/src/services/keyring/prompt/Main.qml")
    qindaqt_enable_warnings(tst_keyring_prompt_qml)
    add_test(NAME keyring_prompt_qml COMMAND tst_keyring_prompt_qml)
    set_tests_properties(keyring_prompt_qml PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QT_FATAL_WARNINGS=1")
endif()

add_subdirectory(pam)

add_test(NAME keyring_prompt_display COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_prompt_display.py" "$<TARGET_FILE:qindaqt-keyring>" "$<TARGET_FILE:qindaqt_pam_fixture_launcher>")
set_tests_properties(keyring_prompt_display PROPERTIES TIMEOUT 90 LABELS "keyring;dbus;private-fixture")

add_test(NAME keyring_native_prompts COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_native_prompts.py" "$<TARGET_FILE:qindaqt-keyring>" "$<TARGET_FILE:qindaqt_keyring_policy_compositor>")
set_tests_properties(keyring_native_prompts PROPERTIES TIMEOUT 90 LABELS "keyring;dbus;private-fixture")

add_executable(tst_keyring_prompt_metadata tst_prompt_metadata.cpp)
target_link_libraries(tst_keyring_prompt_metadata PRIVATE QindaQt::KeyringProtocol Qt6::Test)
qindaqt_enable_warnings(tst_keyring_prompt_metadata)
add_test(NAME keyring_prompt_metadata COMMAND tst_keyring_prompt_metadata)

find_package(Wayland REQUIRED COMPONENTS Server)
find_program(QINDAQT_TEST_WAYLAND_SCANNER wayland-scanner REQUIRED)
pkg_get_variable(QINDAQT_TEST_WAYLAND_PROTOCOL_DATA wayland-protocols pkgdatadir)
set(QINDAQT_TEST_IDLE_XML "${QINDAQT_TEST_WAYLAND_PROTOCOL_DATA}/staging/ext-idle-notify/ext-idle-notify-v1.xml")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-server.h"
 COMMAND ${QINDAQT_TEST_WAYLAND_SCANNER} server-header ${QINDAQT_TEST_IDLE_XML} "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-server.h"
 DEPENDS ${QINDAQT_TEST_IDLE_XML} VERBATIM)
qt_add_executable(tst_keyring_idle tst_idle_observer.cpp "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-server.h")
target_include_directories(tst_keyring_idle PRIVATE "${PROJECT_SOURCE_DIR}/src/services/keyring/daemon" "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(tst_keyring_idle PRIVATE qindaqt_keyring_service Wayland::Server Qt6::Test)
add_test(NAME keyring_idle_observer COMMAND tst_keyring_idle)
set_tests_properties(keyring_idle_observer PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 30)

qt_add_executable(tst_keyring_lock_policy tst_lock_policy.cpp)
target_include_directories(tst_keyring_lock_policy PRIVATE "${PROJECT_SOURCE_DIR}/src/services/keyring/daemon")
target_link_libraries(tst_keyring_lock_policy PRIVATE qindaqt_keyring_service QindaQt::SettingsService Qt6::Test)
target_compile_definitions(tst_keyring_lock_policy PRIVATE QINDAQT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME keyring_lock_policy COMMAND tst_keyring_lock_policy)
set_tests_properties(keyring_lock_policy PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 30)

qt_add_executable(qindaqt_keyring_policy_compositor policy_compositor.cpp "${CMAKE_CURRENT_BINARY_DIR}/ext-idle-notify-server.h")
target_include_directories(qindaqt_keyring_policy_compositor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(qindaqt_keyring_policy_compositor PRIVATE qindaqt_keyring_service Wayland::Server QindaQt::CompositorNames Qt6::Core Qt6::DBus)

add_test(NAME keyring_resident_policy COMMAND ${Python3_EXECUTABLE}
 "${CMAKE_CURRENT_SOURCE_DIR}/test_resident_policy.py" $<TARGET_FILE:qindaqt-keyring>
 $<TARGET_FILE:qindaqt-settings-service> $<TARGET_FILE:qindaqt_keyring_policy_compositor> "${PROJECT_SOURCE_DIR}/data/settings")
set_tests_properties(keyring_resident_policy PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 90 LABELS "keyring;dbus;private-fixture")

qt_add_executable(tst_keyring_collection_import tst_collection_import.cpp)
target_link_libraries(tst_keyring_collection_import PRIVATE QindaQt::KeyringImportStore Qt6::Test)
qindaqt_enable_warnings(tst_keyring_collection_import)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(tst_keyring_collection_import PRIVATE "LINKER:--wrap=fsync")
endif()
add_test(NAME keyring_collection_import COMMAND tst_keyring_collection_import)
set_tests_properties(keyring_collection_import PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 60)

qt_add_executable(tst_keyring_legacy_plan tst_legacy_plan.cpp)
target_link_libraries(tst_keyring_legacy_plan PRIVATE QindaQt::KeyringImportPolicy QindaQt::SecretPortalPolicy Qt6::Test)
qindaqt_enable_warnings(tst_keyring_legacy_plan)
add_test(NAME keyring_legacy_plan COMMAND tst_keyring_legacy_plan)
set_tests_properties(keyring_legacy_plan PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 30)

qt_add_executable(qindaqt_import_verify_fixture import_verify_fixture.cpp)
target_link_libraries(qindaqt_import_verify_fixture PRIVATE QindaQt::KeyringStorage QindaQt::CompositorNames Qt6::Core)
qindaqt_enable_warnings(qindaqt_import_verify_fixture)
add_test(NAME keyring_legacy_import COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/test_legacy_import.py"
    $<TARGET_FILE:qindaqt-keyring-import> $<TARGET_FILE:qindaqt_keyring_policy_compositor> $<TARGET_FILE:qindaqt_import_verify_fixture>)
set_tests_properties(keyring_legacy_import PROPERTIES ENVIRONMENT "QT_FATAL_WARNINGS=1" TIMEOUT 180 LABELS "keyring;dbus;private-fixture")
