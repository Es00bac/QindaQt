# SPDX-License-Identifier: GPL-3.0-or-later
foreach(required IN ITEMS BUILD_DIRECTORY INSTALL_QMLDIR SYSTEM_QMLDIR PROBE SOURCE_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "missing Audio package input: ${required}")
    endif()
endforeach()
set(root "${BUILD_DIRECTORY}/tests/apps/settings/audio/installed-qml")
file(REMOVE_RECURSE "${root}")
foreach(directory IN ITEMS stage deps home config cache data runtime tmp)
    file(MAKE_DIRECTORY "${root}/${directory}")
    file(CHMOD "${root}/${directory}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endforeach()
# Stage only the owning module and its public presentation dependencies.
foreach(subdirectory IN ITEMS design_tokens controls apps/settings/audio)
    execute_process(COMMAND "${CMAKE_COMMAND}" --install
        "${BUILD_DIRECTORY}/src/${subdirectory}" --prefix "${root}/stage"
        --component SettingsAppearanceRuntime
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Audio stage failed ${subdirectory}: ${output}${error}")
    endif()
endforeach()
set(imports "${root}/stage/${INSTALL_QMLDIR}")
set(module "${imports}/QindaQt/SettingsApp/Audio")
file(GLOB dependencies LIST_DIRECTORIES true "${SYSTEM_QMLDIR}/*")
foreach(path IN LISTS dependencies)
    get_filename_component(name "${path}" NAME)
    if(NOT name STREQUAL "QindaQt")
        file(CREATE_LINK "${path}" "${root}/deps/${name}" SYMBOLIC)
    endif()
endforeach()
function(probe mode expected scale suffix)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env
        --unset=QML_IMPORT_PATH --unset=QML2_IMPORT_PATH
        --unset=LD_LIBRARY_PATH --unset=QT_PLUGIN_PATH
        HOME=${root}/home XDG_CONFIG_HOME=${root}/config
        XDG_DATA_HOME=${root}/data XDG_CACHE_HOME=${root}/cache
        XDG_RUNTIME_DIR=${root}/runtime TMPDIR=${root}/tmp
        QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software
        QT_SCALE_FACTOR=${scale} QML_DISABLE_DISK_CACHE=1
        DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent
        DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent
        "${PROBE}" "${imports}" "${root}/deps" "${mode}"
        "${SOURCE_ROOT}/data/themes/qinda-dark.json"
        "${SOURCE_ROOT}/data/icons" "${root}/${suffix}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
        TIMEOUT 30)
    file(WRITE "${root}/${suffix}.log" "${output}${error}")
    if(NOT result STREQUAL "${expected}")
        message(FATAL_ERROR "Audio ${suffix} returned ${result}, expected ${expected}: ${output}${error}")
    endif()
    if(expected EQUAL 0 AND "${output}${error}" MATCHES
       "Unable to assign|ReferenceError|TypeError|failed to load component")
        message(FATAL_ERROR "Audio ${suffix} emitted presentation errors: ${output}${error}")
    endif()
    message(STATUS "Audio ${suffix}: ${result}: ${output}${error}")
endfunction()
probe(compiled 0 1 compiled-dpi1)
probe(compiled 0 2 compiled-dpi2)
file(RENAME "${module}" "${module}.withheld")
probe(compiled 3 1 module-poison)
file(RENAME "${module}.withheld" "${module}")
file(READ "${module}/qmldir" metadata)
file(STRINGS "${module}/qmldir" entries REGEX "^[A-Za-z]+ 1\\.0 .*\\.qml$")
list(LENGTH entries count)
if(NOT count EQUAL 23)
    message(FATAL_ERROR "Audio declared closure count ${count}, expected 23")
endif()
foreach(entry IN LISTS entries)
    string(REGEX REPLACE "^[^ ]+ 1\\.0 " "" path "${entry}")
    if(NOT EXISTS "${module}/${path}")
        message(FATAL_ERROR "Audio declared fallback missing: ${path}")
    endif()
    file(SHA256 "${module}/${path}" installed_hash)
    file(SHA256 "${SOURCE_ROOT}/src/apps/settings/audio/${path}" source_hash)
    if(NOT installed_hash STREQUAL source_hash)
        message(FATAL_ERROR "Audio staged source mismatch: ${path}")
    endif()
endforeach()
string(REGEX REPLACE "prefer [^\n]*\n" "" disk_metadata "${metadata}")
file(WRITE "${module}/qmldir" "${disk_metadata}")
probe(disk 0 1 disk-dpi1)
probe(disk 0 2 disk-dpi2)
foreach(entry IN LISTS entries)
    string(REGEX REPLACE "^[^ ]+ 1\\.0 " "" path "${entry}")
    file(RENAME "${module}/${path}" "${module}/${path}.withheld")
    string(MAKE_C_IDENTIFIER "${path}" poison)
    probe(disk 1 1 "disk-poison-${poison}")
    file(RENAME "${module}/${path}.withheld" "${module}/${path}")
endforeach()
probe(disk 0 1 disk-restored)
file(WRITE "${module}/qmldir" "${metadata}")
probe(compiled 0 1 compiled-restored)
