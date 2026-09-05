# SPDX-License-Identifier: GPL-3.0-or-later

# The pure workspace target may include Qt Core and its own headers only; the
# D-Bus target may additionally include Qt DBus. Neither may name KWin,
# LayerShellQt, the shell runtime, QML, or another applet's headers.
if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "Missing SOURCE_ROOT")
endif()

set(pure_sources
    src/shell/workspaces/include/qindaqt/shell/workspaces/workspace_types.h
    src/shell/workspaces/include/qindaqt/shell/workspaces/workspace_transport.h
    src/shell/workspaces/include/qindaqt/shell/workspaces/workspace_controller.h
    src/shell/workspaces/src/workspace_types.cpp
    src/shell/workspaces/src/workspace_controller.cpp)
set(dbus_sources
    src/shell/workspaces/include/qindaqt/shell/workspaces/qt_workspace_transport.h
    src/shell/workspaces/src/qt_workspace_transport.cpp)

set(forbidden_everywhere
    "<KWin" "kwin/" "LayerShellQt" "QQml" "QQuick" "shellruntimeapplication"
    "task_list" "launcher" "global_menu" "power_applet" "audio_applet"
    "bluetooth_applet" "<QProcess>")
set(forbidden_pure "<QDBus" "QtDBus" "org.kde")

function(check_forbidden relative_path)
    file(READ "${SOURCE_ROOT}/${relative_path}" contents)
    foreach(token IN LISTS ARGN)
        string(FIND "${contents}" "${token}" hit)
        if(NOT hit EQUAL -1)
            message(FATAL_ERROR "${relative_path} must not reference '${token}'")
        endif()
    endforeach()
endfunction()

foreach(source IN LISTS pure_sources)
    check_forbidden("${source}" ${forbidden_everywhere} ${forbidden_pure})
endforeach()
foreach(source IN LISTS dbus_sources)
    check_forbidden("${source}" ${forbidden_everywhere})
endforeach()

# The adapter must bind unique owners, never call the well-known name, and
# must never auto-start a service.
file(READ "${SOURCE_ROOT}/src/shell/workspaces/src/qt_workspace_transport.cpp" adapter)
foreach(required IN ITEMS "GetConnectionUnixProcessID" "setAutoStartService(false)"
                          "compositor-identity-mismatch")
    string(FIND "${adapter}" "${required}" hit)
    if(hit EQUAL -1)
        message(FATAL_ERROR "qt_workspace_transport.cpp lacks required contract: ${required}")
    endif()
endforeach()
foreach(forbidden IN ITEMS "createDesktop" "removeDesktop" "setDesktopName" "killWindow")
    string(FIND "${adapter}" "${forbidden}" hit)
    if(NOT hit EQUAL -1)
        message(FATAL_ERROR "qt_workspace_transport.cpp must not reach ${forbidden}")
    endif()
endforeach()
message(STATUS "Workspace module boundary contracts passed")
