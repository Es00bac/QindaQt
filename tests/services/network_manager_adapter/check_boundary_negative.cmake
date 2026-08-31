# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS BINARY_ROOT POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing N1 boundary-negative input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH BINARY_ROOT OUTPUT_VARIABLE binary_root)
cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
cmake_path(IS_PREFIX binary_root "${poison_root}" NORMALIZE poison_is_in_build)
if(NOT poison_is_in_build OR poison_root STREQUAL binary_root)
    message(FATAL_ERROR "N1 poison root must be a child of the build tree")
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
        message(FATAL_ERROR "N1 boundary checker accepted poison ${relative_path}")
    endif()
    string(CONCAT combined "${output}" "${error}")
    if(NOT combined MATCHES "${expected}")
        message(FATAL_ERROR "N1 checker rejected for wrong reason:\n${combined}")
    endif()
endfunction()

expect_rejection(
    "src/services/network_service/poison.cpp"
    "#include <NetworkManager.h>"
    "Resident Network1 service imports NetworkManager"
)
expect_rejection(
    "src/services/network_qt_transport/poison.cpp"
    "#include <qindaqt/services/network_service/resident_network_service.h>"
    "Qt Network transport crosses service/platform boundary"
)
expect_rejection(
    "src/services/network_manager_adapter/poison.cpp"
    "void leak() { nm_remote_connection_get_secrets(); }"
    "Forbidden secret/process escape"
)
expect_rejection(
    "src/services/network_manager_adapter/include/poison.h"
    "class NMClient;"
    "Public NetworkManager adapter header leaks platform handles"
)
expect_rejection(
    "src/services/network_manager_adapter/data/org.qindaqt.Network1.xml"
    "<arg type=\"a{sv}\"/>"
    "fixed wire was replaced"
)
expect_rejection(
    "src/services/network_manager_adapter/CMakeLists.txt"
    "install(FILES poison COMPONENT WrongNetworkPackage)"
    "N1 package registry is incomplete"
)

file(REMOVE_RECURSE "${poison_root}")
message(STATUS "Network N1 boundary checker rejected all injected poisons")
