# AGENT-CONTRACT: This row proves the Terminal component's installed surface:
# exact staged files, desktop entry, no per-app theme catalog (ADR-0116), and
# that the installed executable runs its argv gate from the staged prefix with
# only the audited external qtermwidget dependency resolvable. The positional
# rejection exits before any window, session, or bus access exists, so this
# gate never opens a display or starts a PTY. Launch/PSS/first-frame
# qualification is serialized compiler-lane work and is not claimed here.
if(NOT INSTALL_PREFIX MATCHES "/installed-terminal-stage$")
    message(FATAL_ERROR "refusing unsafe staged-install prefix: '${INSTALL_PREFIX}'")
endif()
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
)
foreach(relative_path ${required_paths})
    if(NOT EXISTS "${STAGE_PREFIX}/${relative_path}")
        message(FATAL_ERROR "staged Terminal install is missing: ${relative_path}")
    endif()
endforeach()

# ADR-0116: the Terminal component ships no per-app theme catalog and no
# Tokens payload; the Qt platform theme owns appearance.
foreach(forbidden_path
        "${INSTALL_DATADIR}/qindaqt/themes")
    if(EXISTS "${STAGE_PREFIX}/${forbidden_path}")
        message(FATAL_ERROR
            "staged Terminal install must not ship a per-app theme catalog: ${forbidden_path}")
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
# The external library may share /usr/lib64 with a previously installed QindaQt.
# Copy only the exact audited dependency into the stage; adding its whole host
# directory to LD_LIBRARY_PATH would silently override staged application libs.
set(qtermwidget_library_directory "${STAGE_PREFIX}/audited-dependencies")
file(MAKE_DIRECTORY "${qtermwidget_library_directory}")
file(COPY_FILE "${QTERMWIDGET_LIBRARY}"
     "${qtermwidget_library_directory}/libqtermwidget6.so.2")

set(ENV{QT_QPA_PLATFORM} "offscreen")
# Strip ambient data roots so only the staged prefix can satisfy resolution.
set(ENV{XDG_DATA_HOME} "${STAGE_PREFIX}/empty-xdg-data-home")
set(ENV{HOME} "${STAGE_PREFIX}/empty-home")
# Strip ambient dynamic-loader state while retaining the one audited external
# dependency that a normal Linux package manager installs system-wide.
set(ENV{LD_LIBRARY_PATH} "${qtermwidget_library_directory}")
file(MAKE_DIRECTORY "${STAGE_PREFIX}/empty-xdg-data-home")
file(MAKE_DIRECTORY "${STAGE_PREFIX}/empty-home")

# The argv gate proves the staged executable loads and runs with staged
# libraries only; it exits before Settings1, any window, or any PTY.
execute_process(
    COMMAND "${STAGE_PREFIX}/${INSTALL_BINDIR}/qindaqt-terminal"
            unexpected-positional
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_output
    ERROR_VARIABLE probe_error
)
if(NOT probe_result EQUAL 2)
    message(FATAL_ERROR
        "staged argv rejection did not exit 2 (${probe_result}): ${probe_output}${probe_error}")
endif()
if(NOT probe_error MATCHES "unexpected positional arguments")
    message(FATAL_ERROR "staged argv rejection diagnostic mismatch: ${probe_error}")
endif()

file(REMOVE_RECURSE "${STAGE_PREFIX}")
