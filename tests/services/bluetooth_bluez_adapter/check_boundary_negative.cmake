# SPDX-License-Identifier: GPL-3.0-or-later

# Negative control: proves the boundary checker fails on the unrepaired tree
# for each forbidden pattern, and for the right reason.

foreach(required IN ITEMS BINARY_ROOT POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing Bluetooth B1 boundary-negative input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH BINARY_ROOT OUTPUT_VARIABLE binary_root)
cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
cmake_path(IS_PREFIX binary_root "${poison_root}" NORMALIZE poison_is_in_build)
if(NOT poison_is_in_build OR poison_root STREQUAL binary_root)
    message(FATAL_ERROR "Bluetooth B1 poison root must be a child of the build tree")
endif()

function(expect_rejection relative_path source_text expected)
    file(REMOVE_RECURSE "${poison_root}")
    cmake_path(GET relative_path PARENT_PATH parent)
    file(MAKE_DIRECTORY "${poison_root}/${parent}")
    file(WRITE "${poison_root}/${relative_path}" "${source_text}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${poison_root}"
                -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(status EQUAL 0)
        message(FATAL_ERROR "Bluetooth B1 boundary checker accepted poison ${relative_path}")
    endif()
    string(CONCAT combined "${output}" "${error}")
    if(NOT combined MATCHES "${expected}")
        message(FATAL_ERROR "Bluetooth B1 checker rejected for wrong reason:\n${combined}")
    endif()
endfunction()

expect_rejection(
    "src/services/bluetooth_bluez_adapter/src/poison.cpp"
    "#include <bluez-qt/manager.h>"
    "reaches BlueZ through BluezQt"
)
expect_rejection(
    "src/services/bluetooth_bluez_adapter/src/poison.cpp"
    "#include <qindaqt/services/bluetooth_service/resident_bluetooth_service.h>"
    "reverses module direction"
)
expect_rejection(
    "src/services/bluetooth_bluez_adapter/src/poison.cpp"
    "void pair() { call(QStringLiteral(\"Pair\")); }"
    "pairing/trust/record authority"
)
expect_rejection(
    "src/services/bluetooth_bluez_adapter/include/poison.h"
    "#include \"../src/bluez_transport.h\""
    "leaks private internals"
)
expect_rejection(
    "tests/services/bluetooth_bluez_adapter/poison.cpp"
    "QDBusConnection bus = QDBusConnection::systemBus();"
    "contacts an ambient bus"
)
expect_rejection(
    "src/services/bluetooth_service/app/main.cpp"
    "int main() { return 0; }"
    "lost explicit backend selection"
)
expect_rejection(
    "src/services/bluetooth_service/CMakeLists.txt"
    "add_executable(poison)"
    "does not link the production adapter"
)
expect_rejection(
    "src/services/bluetooth_bluez_adapter/CMakeLists.txt"
    "install(TARGETS poison)"
    "package registry is incomplete"
)

file(REMOVE_RECURSE "${poison_root}")
message(STATUS "Bluetooth B1 boundary checker rejected all injected poisons")
