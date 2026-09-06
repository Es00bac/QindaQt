# SPDX-License-Identifier: GPL-3.0-or-later

foreach(_required IN ITEMS AGENT_INPUT_BUILD_ROOT AGENT_INPUT_INSTALL_ROOT AGENT_INPUT_BINDIR)
    if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
        message(FATAL_ERROR "${_required} is required")
    endif()
endforeach()

file(REMOVE_RECURSE "${AGENT_INPUT_INSTALL_ROOT}")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}" --install "${AGENT_INPUT_BUILD_ROOT}"
        --prefix "${AGENT_INPUT_INSTALL_ROOT}" --component AgentInput
    RESULT_VARIABLE _install_result
    OUTPUT_VARIABLE _install_stdout
    ERROR_VARIABLE _install_stderr
)
if(NOT _install_result EQUAL 0)
    message(
        FATAL_ERROR
        "AgentInput install failed (${_install_result})\n${_install_stdout}\n${_install_stderr}"
    )
endif()

set(_tool "${AGENT_INPUT_INSTALL_ROOT}/${AGENT_INPUT_BINDIR}/qindaqt-agent-input")
set(_package "${AGENT_INPUT_INSTALL_ROOT}/${AGENT_INPUT_BINDIR}/agent_input/__init__.py")
if(NOT EXISTS "${_tool}" OR NOT EXISTS "${_package}")
    message(FATAL_ERROR "installed agent-input CLI/package is incomplete")
endif()

# --help imports the installed package and both runtime bindings, while the
# parser exits before opening a bus or requesting portal approval.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=" "${_tool}" --help
    RESULT_VARIABLE _help_result
    OUTPUT_VARIABLE _help_stdout
    ERROR_VARIABLE _help_stderr
)
if(NOT _help_result EQUAL 0)
    message(FATAL_ERROR "installed qindaqt-agent-input --help failed (${_help_result})\n${_help_stderr}")
endif()
if(NOT _help_stdout MATCHES "RemoteDesktop")
    message(FATAL_ERROR "installed qindaqt-agent-input help is missing its portal description")
endif()

message(STATUS "installed qindaqt-agent-input smoke passed")
