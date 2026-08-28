# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS BINARY_ROOT POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing boundary-negative input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH BINARY_ROOT OUTPUT_VARIABLE binary_root)
cmake_path(NORMAL_PATH POISON_ROOT OUTPUT_VARIABLE poison_root)
cmake_path(IS_PREFIX binary_root "${poison_root}" NORMALIZE poison_is_in_build)
if(NOT poison_is_in_build OR poison_root STREQUAL binary_root)
    message(FATAL_ERROR "Boundary poison root must be a child of the build tree")
endif()

function(expect_policy_rejection module source_text expected)
    file(REMOVE_RECURSE "${poison_root}")
    set(module_root "${poison_root}/src/services/${module}")
    file(MAKE_DIRECTORY "${module_root}")
    file(WRITE "${module_root}/poison.cpp" "${source_text}\n")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DSOURCE_ROOT=${poison_root}"
                -P "${CHECK_SCRIPT}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(status EQUAL 0)
        message(FATAL_ERROR
                "Network boundary checker accepted ${module} poison")
    endif()
    string(CONCAT combined "${output}" "${error}")
    if(NOT combined MATCHES "${expected}")
        message(FATAL_ERROR
                "Network boundary checker failed for the wrong reason:\n${combined}")
    endif()
endfunction()

expect_policy_rejection(
    network_client
    "#include <QtDBus/QDBusConnection>"
    "Forbidden network-network_client dependency"
)
expect_policy_rejection(
    network_model
    "#include <QtCore/QTimer>"
    "Forbidden impurity in network model"
)
expect_policy_rejection(
    network_protocol
    "// NetworkManager authority must not enter N0"
    "Forbidden network-network_protocol dependency"
)

file(REMOVE_RECURSE "${poison_root}")
message(STATUS "Network boundary checker rejected all injected poisons")
