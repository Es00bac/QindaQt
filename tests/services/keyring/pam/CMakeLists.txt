# SPDX-License-Identifier: GPL-3.0-or-later
set(PAM_SOURCE "${PROJECT_SOURCE_DIR}/src/services/keyring/pam")
function(pam_fixture target)
    target_include_directories(${target} PRIVATE "${PAM_SOURCE}")
    target_link_libraries(${target} PRIVATE QindaQt::KeyringStorage)
    target_compile_definitions(${target} PRIVATE QINDAQT_PAM_FIXTURE
        QINDAQT_PAM_HELPER="$<TARGET_FILE:qindaqt_pam_fixture_helper>" QINDAQT_KEYRING_DAEMON="$<TARGET_FILE:qindaqt-keyring>")
    qindaqt_enable_warnings(${target})
endfunction()
add_executable(qindaqt_pam_fixture_helper "${PAM_SOURCE}/helper.cpp" "${PAM_SOURCE}/transport.cpp" fixture_owner.cpp)
pam_fixture(qindaqt_pam_fixture_helper)
add_library(pam_qindaqt_fixture MODULE "${PAM_SOURCE}/module.cpp" "${PAM_SOURCE}/bridge_process.cpp" "${PAM_SOURCE}/transport.cpp" fixture_owner.cpp)
set_target_properties(pam_qindaqt_fixture PROPERTIES PREFIX "")
pam_fixture(pam_qindaqt_fixture)
target_link_libraries(pam_qindaqt_fixture PRIVATE PkgConfig::PAM)
add_executable(qindaqt_pam_fixture_launcher "${PAM_SOURCE}/launcher.cpp" "${PAM_SOURCE}/transport.cpp")
pam_fixture(qindaqt_pam_fixture_launcher)
add_library(pam_qindaqt_tokens MODULE fixture_tokens.cpp)
set_target_properties(pam_qindaqt_tokens PROPERTIES PREFIX "")
target_link_libraries(pam_qindaqt_tokens PRIVATE PkgConfig::PAM)
qindaqt_enable_warnings(pam_qindaqt_tokens)
add_library(qindaqt_pam_injection MODULE injection.cpp)
qindaqt_enable_warnings(qindaqt_pam_injection)
add_executable(qindaqt_pam_driver driver.cpp)
target_link_libraries(qindaqt_pam_driver PRIVATE PkgConfig::PAM)
qindaqt_enable_warnings(qindaqt_pam_driver)
add_test(NAME keyring_pam_integration COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_pam.py"
    "$<TARGET_FILE:qindaqt-keyring>" "$<TARGET_FILE:pam_qindaqt_fixture>" "$<TARGET_FILE:pam_qindaqt_tokens>"
    "$<TARGET_FILE:qindaqt_pam_driver>" "$<TARGET_FILE:qindaqt_pam_fixture_launcher>" "$<TARGET_FILE:qindaqt_pam_injection>" "$<TARGET_FILE:qindaqt_pam_pidfd_unavailable>")
set_tests_properties(keyring_pam_integration PROPERTIES TIMEOUT 150 LABELS "keyring;pam;private-fixture")

add_executable(qindaqt_pam_owner_policy test_owner_policy.cpp "${PAM_SOURCE}/transport.cpp")
pam_fixture(qindaqt_pam_owner_policy)
add_test(NAME keyring_pam_owner_policy COMMAND qindaqt_pam_owner_policy)
set_tests_properties(keyring_pam_owner_policy PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_library(qindaqt_pam_pidfd_unavailable MODULE pidfd_unavailable.cpp)
target_link_libraries(qindaqt_pam_pidfd_unavailable PRIVATE "${CMAKE_DL_LIBS}")
qindaqt_enable_warnings(qindaqt_pam_pidfd_unavailable)

# The exact messages sent to the systemd manager, checked on a private bus that
# loads the REAL installed systemd1 policy (skipped, exit 77, when absent).
find_package(Threads REQUIRED)
find_program(DBUS_DAEMON_EXECUTABLE dbus-daemon REQUIRED)
find_file(QINDAQT_SYSTEMD1_BUS_POLICY org.freedesktop.systemd1.conf
    PATHS /usr/share/dbus-1/system.d NO_DEFAULT_PATH)
if(NOT QINDAQT_SYSTEMD1_BUS_POLICY)
    set(QINDAQT_SYSTEMD1_BUS_POLICY "")
endif()
add_executable(qindaqt_pam_manager_calls test_manager_calls.cpp "${PAM_SOURCE}/manager_calls.cpp")
target_include_directories(qindaqt_pam_manager_calls PRIVATE "${PAM_SOURCE}")
target_link_libraries(qindaqt_pam_manager_calls PRIVATE PkgConfig::SYSTEMD Threads::Threads)
target_compile_definitions(qindaqt_pam_manager_calls PRIVATE QINDAQT_DBUS_DAEMON="${DBUS_DAEMON_EXECUTABLE}"
    QINDAQT_SYSTEMD1_BUS_POLICY="${QINDAQT_SYSTEMD1_BUS_POLICY}")
qindaqt_enable_warnings(qindaqt_pam_manager_calls)
add_test(NAME keyring_pam_manager_calls COMMAND qindaqt_pam_manager_calls)
set_tests_properties(keyring_pam_manager_calls PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77
    LABELS "keyring;pam;dbus;private-fixture")
