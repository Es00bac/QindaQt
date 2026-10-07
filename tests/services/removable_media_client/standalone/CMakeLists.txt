# SPDX-License-Identifier: LGPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(QindaQtMediaInventoryTests LANGUAGES CXX)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
find_package(Qt6 6.11 REQUIRED COMPONENTS Core DBus Test)
get_filename_component(QINDAQT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
function(qindaqt_enable_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endfunction()
add_subdirectory("${QINDAQT_ROOT}/src/services/removable_media_protocol" protocol)
add_subdirectory("${QINDAQT_ROOT}/src/services/removable_media_client" client)
qt_add_library(qindaqt_removable_media_core STATIC
    "${QINDAQT_ROOT}/src/apps/removable_media/media_types.cpp"
    "${QINDAQT_ROOT}/src/apps/removable_media/media_backend.h"
    "${QINDAQT_ROOT}/src/apps/removable_media/media_preferences.cpp"
    "${QINDAQT_ROOT}/src/apps/removable_media/media_controller.cpp"
    "${QINDAQT_ROOT}/src/apps/removable_media/media_exporter.cpp"
    "${QINDAQT_ROOT}/src/apps/removable_media/media_public_projection.cpp"
    "${QINDAQT_ROOT}/src/apps/removable_media/udisks_backend.cpp"
    "${QINDAQT_ROOT}/src/apps/removable_media/udisks_operations.cpp"
    "${QINDAQT_ROOT}/src/apps/removable_media/media_notifications.cpp")
target_include_directories(qindaqt_removable_media_core PUBLIC "${QINDAQT_ROOT}/src/apps/removable_media")
target_link_libraries(qindaqt_removable_media_core PUBLIC QindaQt::RemovableMediaProtocol Qt6::Core Qt6::DBus)
qindaqt_enable_warnings(qindaqt_removable_media_core)
add_subdirectory(.. tests)

foreach(behavior policy udisks notifications)
    qt_add_executable(qindaqt_media_${behavior}_tests
        "${QINDAQT_ROOT}/src/apps/removable_media/tests/tst_media_${behavior}.cpp")
    target_link_libraries(qindaqt_media_${behavior}_tests PRIVATE qindaqt_removable_media_core Qt6::Test)
    qindaqt_enable_warnings(qindaqt_media_${behavior}_tests)
    if(behavior STREQUAL "policy")
        add_test(NAME qindaqt.removable-media-policy COMMAND qindaqt_media_${behavior}_tests)
    else()
        add_test(NAME qindaqt.removable-media-${behavior}
            COMMAND dbus-run-session -- $<TARGET_FILE:qindaqt_media_${behavior}_tests>)
        set_tests_properties(qindaqt.removable-media-${behavior} PROPERTIES
            ENVIRONMENT "DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent" TIMEOUT 60)
    endif()
endforeach()
