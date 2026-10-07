# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS BUILD_DIRECTORY INSTALL_QMLDIR PROBE SYSTEM_QMLDIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing Network package input: ${required}")
    endif()
endforeach()
set(root "${BUILD_DIRECTORY}/tests/apps/settings/network/installed-qml")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}/deps/QindaQt" "${root}/runtime")
file(CHMOD "${root}/runtime" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
execute_process(COMMAND "${CMAKE_COMMAND}" --install
    "${BUILD_DIRECTORY}/src/apps/settings/network" --prefix "${root}/stage"
    --component SettingsAppearanceRuntime RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Network stage failed: ${output}${error}")
endif()
set(imports "${root}/stage/${INSTALL_QMLDIR}")
set(module "${imports}/QindaQt/SettingsApp/Network")
# Public dependency modules are installed read-only. Deliberately exclude the
# system QindaQt root so the installed Network cannot rescue this candidate.
file(GLOB dependencies LIST_DIRECTORIES true "${SYSTEM_QMLDIR}/*")
foreach(path IN LISTS dependencies)
    get_filename_component(name "${path}" NAME)
    if(NOT name STREQUAL "QindaQt")
        file(CREATE_LINK "${path}" "${root}/deps/${name}" SYMBOLIC)
    endif()
endforeach()
foreach(name IN ITEMS Tokens Controls)
    file(CREATE_LINK "${SYSTEM_QMLDIR}/QindaQt/${name}"
        "${imports}/QindaQt/${name}" SYMBOLIC)
    file(CREATE_LINK "${SYSTEM_QMLDIR}/QindaQt/${name}"
        "${root}/deps/QindaQt/${name}" SYMBOLIC)
endforeach()
function(probe mode expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        --unset=QML_IMPORT_PATH --unset=QML2_IMPORT_PATH
        --unset=LD_LIBRARY_PATH --unset=QT_PLUGIN_PATH
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
        QML_DISABLE_DISK_CACHE=1 XDG_RUNTIME_DIR=${root}/runtime
        DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent
        DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent
        "${PROBE}" "${imports}" "${root}/deps" "${mode}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(expected STREQUAL "pass" AND NOT result EQUAL 0)
        message(FATAL_ERROR "Network ${mode} failed: ${output}${error}")
    elseif(expected STREQUAL "fail")
        if(mode STREQUAL "disk")
            set(expected_result 1)
        else()
            set(expected_result 3)
        endif()
        if(NOT result EQUAL expected_result)
            message(FATAL_ERROR
                "Network ${mode} poison returned ${result}, expected ${expected_result}: ${output}${error}")
        endif()
    endif()
    message(STATUS "Network ${mode} ${expected}: ${result}: ${output}${error}")
endfunction()
probe(compiled pass)
file(RENAME "${module}" "${module}.withheld")
probe(compiled fail)
file(RENAME "${module}.withheld" "${module}")
# Disk fallback must not be redirected to the compiled resources by prefer.
file(READ "${module}/qmldir" metadata)
string(REGEX REPLACE "prefer [^\n]*\n" "" disk_metadata "${metadata}")
file(WRITE "${module}/qmldir" "${disk_metadata}")
probe(disk pass)
file(STRINGS "${module}/qmldir" entries REGEX "^[A-Za-z]+ 1\.0 qml/.*\.qml$")
foreach(entry IN LISTS entries)
    string(REGEX REPLACE "^[^ ]+ 1\.0 " "" path "${entry}")
    if(NOT EXISTS "${module}/${path}")
        message(FATAL_ERROR "Network declared fallback missing: ${path}")
    endif()
    file(RENAME "${module}/${path}" "${module}/${path}.withheld")
    probe(disk fail)
    file(RENAME "${module}/${path}.withheld" "${module}/${path}")
endforeach()
probe(disk pass)
file(WRITE "${module}/qmldir" "${metadata}")
probe(compiled pass)
