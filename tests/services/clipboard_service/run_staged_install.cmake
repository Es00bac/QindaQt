# SPDX-License-Identifier: GPL-3.0-or-later

foreach(required IN ITEMS QINDAQT_CMAKE QINDAQT_BUILD_DIRECTORY QINDAQT_INSTALL_PREFIX
                          QINDAQT_CONSUMER_SOURCE_DIRECTORY QINDAQT_CONSUMER_SOURCE
                          QINDAQT_INSTALL_BINDIR QINDAQT_INSTALL_LIBDIR
                          QINDAQT_INSTALL_INCLUDEDIR QINDAQT_INSTALL_DATADIR
                          QINDAQT_INSTALL_SYSTEMDUSERUNITDIR
                          QINDAQT_CLIPBOARD_PROTOCOL_LIBRARY_NAME
                          QINDAQT_CLIPBOARD_MODEL_LIBRARY_NAME
                          QINDAQT_CLIPBOARD_CLIENT_LIBRARY_NAME
                          QINDAQT_CLIPBOARD_ADAPTER_LIBRARY_NAME
                          QINDAQT_CLIPBOARD_SERVICE_LIBRARY_NAME QINDAQT_QT6_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing staged-install test input: ${required}")
    endif()
endforeach()

cmake_path(NORMAL_PATH QINDAQT_BUILD_DIRECTORY OUTPUT_VARIABLE build_directory)
cmake_path(NORMAL_PATH QINDAQT_INSTALL_PREFIX OUTPUT_VARIABLE install_prefix)
cmake_path(IS_PREFIX build_directory "${install_prefix}" NORMALIZE prefix_is_in_build)
if(NOT prefix_is_in_build OR install_prefix STREQUAL build_directory)
    message(FATAL_ERROR "Refusing to replace a staged prefix outside the test build tree")
endif()

# AGENT-GUARD: A clean stage detects deleted install rules. The prefix check
# above confines removal to this test's build tree.
file(REMOVE_RECURSE "${install_prefix}")
foreach(module IN ITEMS clipboard_model clipboard_protocol clipboard_client
                        clipboard_wayland_adapter clipboard_service)
    set(install_script
        "${build_directory}/src/services/${module}/cmake_install.cmake")
    execute_process(
        COMMAND "${QINDAQT_CMAKE}" "-DCMAKE_INSTALL_PREFIX=${install_prefix}"
                "-DCMAKE_INSTALL_CONFIG_NAME=${QINDAQT_BUILD_TYPE}"
                -P "${install_script}"
        RESULT_VARIABLE install_status OUTPUT_VARIABLE install_output
        ERROR_VARIABLE install_error)
    if(NOT install_status EQUAL 0)
        message(FATAL_ERROR
            "Staged ${module} install failed:\n${install_output}${install_error}")
    endif()
endforeach()

set(staged_executable "${install_prefix}/${QINDAQT_INSTALL_BINDIR}/qindaqt-clipboard-host")
set(staged_activation "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/dbus-1/services/org.qindaqt.Clipboard1.service")
set(staged_unit "${install_prefix}/${QINDAQT_INSTALL_SYSTEMDUSERUNITDIR}/qindaqt-clipboard-host.service")
set(staged_xml "${install_prefix}/${QINDAQT_INSTALL_DATADIR}/dbus-1/interfaces/org.qindaqt.Clipboard1.xml")
set(staged_protocol "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}/${QINDAQT_CLIPBOARD_PROTOCOL_LIBRARY_NAME}")
set(staged_model "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}/${QINDAQT_CLIPBOARD_MODEL_LIBRARY_NAME}")
set(staged_client "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}/${QINDAQT_CLIPBOARD_CLIENT_LIBRARY_NAME}")
set(staged_adapter "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}/${QINDAQT_CLIPBOARD_ADAPTER_LIBRARY_NAME}")
set(staged_service "${install_prefix}/${QINDAQT_INSTALL_LIBDIR}/${QINDAQT_CLIPBOARD_SERVICE_LIBRARY_NAME}")
foreach(staged IN ITEMS staged_executable staged_activation staged_unit staged_xml
                        staged_protocol staged_model staged_client staged_adapter
                        staged_service)
    if(NOT EXISTS "${${staged}}")
        message(FATAL_ERROR "Staged Clipboard1 artifact missing: ${${staged}}")
    endif()
endforeach()
foreach(configured IN ITEMS staged_activation staged_unit)
    file(READ "${${configured}}" configured_content)
    if(configured_content MATCHES "@[A-Za-z0-9_]+@")
        message(FATAL_ERROR "Staged ${${configured}} contains an @...@ placeholder")
    endif()
endforeach()
file(READ "${staged_unit}" unit_content)
if(NOT unit_content MATCHES "ProtectHome=read-only")
    message(FATAL_ERROR "Clipboard1 unit must retain access to /run/user sockets")
endif()
if(unit_content MATCHES "ProtectHome=true")
    message(FATAL_ERROR "Clipboard1 unit hides its required /run/user sockets")
endif()

set(consumer_build "${install_prefix}/consumer-build")
execute_process(
    COMMAND "${QINDAQT_CMAKE}" -S "${QINDAQT_CONSUMER_SOURCE_DIRECTORY}"
            -B "${consumer_build}" -G "${QINDAQT_GENERATOR}"
            "-DCMAKE_BUILD_TYPE=${QINDAQT_BUILD_TYPE}" "-DQt6_DIR=${QINDAQT_QT6_DIR}"
            "-DQINDAQT_STAGE_INCLUDE_DIR=${install_prefix}/${QINDAQT_INSTALL_INCLUDEDIR}"
            "-DQINDAQT_CLIPBOARD_PROTOCOL_LIBRARY=${staged_protocol}"
            "-DQINDAQT_CLIPBOARD_MODEL_LIBRARY=${staged_model}"
            "-DQINDAQT_CONSUMER_SOURCE=${QINDAQT_CONSUMER_SOURCE}"
    RESULT_VARIABLE configure_status OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error)
if(NOT configure_status EQUAL 0)
    message(FATAL_ERROR "Installed Clipboard consumer configure failed:\n${configure_output}${configure_error}")
endif()
execute_process(COMMAND "${QINDAQT_CMAKE}" --build "${consumer_build}" --parallel 2
                RESULT_VARIABLE build_status OUTPUT_VARIABLE build_output
                ERROR_VARIABLE build_error)
if(NOT build_status EQUAL 0)
    message(FATAL_ERROR "Installed Clipboard consumer build failed:\n${build_output}${build_error}")
endif()
execute_process(COMMAND "${consumer_build}/qindaqt_installed_clipboard_consumer"
                RESULT_VARIABLE consumer_status OUTPUT_VARIABLE consumer_output
                ERROR_VARIABLE consumer_error)
if(NOT consumer_status EQUAL 0)
    message(FATAL_ERROR "Installed Clipboard consumer failed ${consumer_status}:\n${consumer_output}${consumer_error}")
endif()
