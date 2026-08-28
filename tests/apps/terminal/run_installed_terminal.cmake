# AGENT-CONTRACT: This row proves the Terminal component's installed surface:
# exact staged files, desktop entry, and that the installed executable
# resolves themes from the installed prefix (not the source or build tree).
# --check-theme exits before any window or session exists, so this gate never
# opens a display or starts a PTY. Launch/PSS/first-frame qualification is
# serialized compiler-lane work and is deliberately not claimed here.
set(STAGE_PREFIX "${INSTALL_PREFIX}")

file(REMOVE_RECURSE "${STAGE_PREFIX}")
file(MAKE_DIRECTORY "${STAGE_PREFIX}")

execute_process(
    COMMAND "${CMAKE_COMMAND}"
            --install "${BUILD_DIRECTORY}"
            --prefix "${STAGE_PREFIX}"
            --component Terminal
            --config "${CONFIGURATION}"
    RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "Terminal component install failed: ${install_result}")
endif()

set(required_paths
    "${INSTALL_BINDIR}/qindaqt-terminal"
    "${INSTALL_DATADIR}/applications/org.qindaqt.Terminal.desktop"
    "${INSTALL_DATADIR}/qindaqt/themes/qinda-dark.json"
)
foreach(relative_path ${required_paths})
    if(NOT EXISTS "${STAGE_PREFIX}/${relative_path}")
        message(FATAL_ERROR "staged Terminal install is missing: ${relative_path}")
    endif()
endforeach()

# AGENT-CONTRACT: qtermwidget is an external system dependency rather than a
# Terminal-component payload. The staged probe must use the exact dependency
# resolved by CMake, because the installed application deliberately has only a
# relocatable $ORIGIN RPATH and ambient loader paths are not acceptance proof.
if(NOT IS_ABSOLUTE "${QTERMWIDGET_LIBRARY}" OR
   NOT EXISTS "${QTERMWIDGET_LIBRARY}")
    message(FATAL_ERROR
        "qtermwidget dependency is not an exact existing file: ${QTERMWIDGET_LIBRARY}")
endif()
get_filename_component(
    qtermwidget_library_directory "${QTERMWIDGET_LIBRARY}" DIRECTORY
)

set(ENV{QT_QPA_PLATFORM} "offscreen")
# Strip ambient theme roots so only the staged prefix can satisfy resolution.
set(ENV{XDG_DATA_HOME} "${STAGE_PREFIX}/empty-xdg-data-home")
set(ENV{HOME} "${STAGE_PREFIX}/empty-home")
# Strip ambient dynamic-loader state while retaining the one audited external
# dependency that a normal Linux package manager installs system-wide.
set(ENV{LD_LIBRARY_PATH} "${qtermwidget_library_directory}")
file(MAKE_DIRECTORY "${STAGE_PREFIX}/empty-xdg-data-home")
file(MAKE_DIRECTORY "${STAGE_PREFIX}/empty-home")

execute_process(
    COMMAND "${STAGE_PREFIX}/${INSTALL_BINDIR}/qindaqt-terminal"
            --check-theme --theme qinda-dark
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
)
if(NOT probe_result EQUAL 0)
    message(FATAL_ERROR "staged --check-theme failed (${probe_result}): ${probe_output}${probe_error}")
endif()
if(NOT probe_output MATCHES "qinda-dark qst-1")
    message(FATAL_ERROR "staged theme identity mismatch: ${probe_output}")
endif()

file(REMOVE_RECURSE "${STAGE_PREFIX}")
