# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS CHECK_SCRIPT MODULE_SOURCE_DIR STAGE_DIR
                          EXPECTED_DEVICE_SHA256 EXPECTED_MANAGEMENT_SHA256)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing Display writer poison input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH STAGE_DIR OUTPUT_VARIABLE stage)
cmake_path(NORMAL_PATH MODULE_SOURCE_DIR OUTPUT_VARIABLE module_root)
cmake_path(IS_PREFIX module_root "${stage}" NORMALIZE stage_inside_module)
if(stage_inside_module OR NOT IS_ABSOLUTE "${stage}")
    message(FATAL_ERROR "Poison stage must be absolute and outside the source module")
endif()

file(REMOVE_RECURSE "${stage}")
file(MAKE_DIRECTORY "${stage}/src/services")
file(COPY "${module_root}" DESTINATION "${stage}/src/services")
file(APPEND
    "${stage}/src/services/display_writer/include/qindaqt/services/display_writer/output_management_port.h"
    "\n#include <QtWaylandClient/QWaylandClientExtension> // poison probe\n"
)

execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${stage}"
        "-DEXPECTED_DEVICE_SHA256=${EXPECTED_DEVICE_SHA256}"
        "-DEXPECTED_MANAGEMENT_SHA256=${EXPECTED_MANAGEMENT_SHA256}"
        -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE poison_status
    OUTPUT_VARIABLE poison_output
    ERROR_VARIABLE poison_error
)
if(poison_status EQUAL 0)
    message(FATAL_ERROR
        "Display writer boundary accepted a planted public platform dependency:\n"
        "${poison_output}${poison_error}")
endif()

# A second poison proves the private-source scan cannot be bypassed by choosing
# another accepted C++ extension.
file(REMOVE_RECURSE "${stage}")
file(MAKE_DIRECTORY "${stage}/src/services")
file(COPY "${module_root}" DESTINATION "${stage}/src/services")
file(WRITE "${stage}/src/services/display_writer/src/extension_poison.cc"
     "#include <QtDBus/QDBusConnection> // extension poison probe\n")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSOURCE_ROOT=${stage}"
        "-DEXPECTED_DEVICE_SHA256=${EXPECTED_DEVICE_SHA256}"
        "-DEXPECTED_MANAGEMENT_SHA256=${EXPECTED_MANAGEMENT_SHA256}"
        -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE extension_poison_status
    OUTPUT_VARIABLE extension_poison_output
    ERROR_VARIABLE extension_poison_error
)
if(extension_poison_status EQUAL 0)
    message(FATAL_ERROR
        "Display writer boundary accepted a planted private .cc dependency:\n"
        "${extension_poison_output}${extension_poison_error}")
endif()

message(STATUS
    "Display writer boundary rejects public-header and alternate-extension poisons")
file(REMOVE_RECURSE "${stage}")
