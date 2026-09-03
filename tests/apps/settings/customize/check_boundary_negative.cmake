# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS POISON_ROOT CHECK_SCRIPT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing boundary poison input: ${required}")
    endif()
endforeach()

file(REMOVE_RECURSE "${POISON_ROOT}")
file(MAKE_DIRECTORY "${POISON_ROOT}")
file(WRITE "${POISON_ROOT}/poison.cpp"
     "#include <LayerShellQt/Shell>\n#include <QDBusConnection>\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -DSCAN_ROOT=${POISON_ROOT} -P "${CHECK_SCRIPT}"
    RESULT_VARIABLE poison_status
    OUTPUT_VARIABLE poison_output
    ERROR_VARIABLE poison_error
)
if(poison_status EQUAL 0)
    message(FATAL_ERROR
        "Customize boundary poison was accepted:\n${poison_output}${poison_error}")
endif()
file(REMOVE_RECURSE "${POISON_ROOT}")
